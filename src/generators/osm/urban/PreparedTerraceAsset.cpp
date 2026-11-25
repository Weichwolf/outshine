#include "PreparedTerraceAsset.h"
#include "AssetCache.h"
#include "BinaryValueArchive.h"
#include "FootprintTerraces.h"
#include "Geodesy.h"
#include "PreparedBuildingAssets.h"
#include "Sha256.h"
#include "TangentFrame.h"
#include "TileGeodesy.h"
#include "UrbanPrismAsset.h"
#include "Units.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr uint64_t kRemainderFormat = 0x3154534452524554ULL;

std::string RemainderKey(const std::string &request) {
  return Sha256Hex("prepared-terrace-remainder-1/" + request);
}

bool ValidAnchor(LongitudeLatitude at) {
  return std::abs(at.LatitudeDeg) <= kDegPerHalfTurn / 2 &&
         std::abs(at.LongitudeDeg) <= kDegPerHalfTurn;
}

bool ReadRing(std::span<const double> points,
              GeographicRing ring,
              const TangentFrame &frame,
              PolygonRing &into) {
  const size_t total = points.size() / 2;
  if (ring.First > total || ring.Count > total - ring.First || ring.Count < 3) { return false; }
  const size_t first = static_cast<size_t>(ring.First) * 2;
  const size_t count = static_cast<size_t>(ring.Count) * 2;
  const auto coordinates = points.subspan(first, count);
  into.reserve(ring.Count);
  for (size_t at = 0; at < coordinates.size(); at += 2) {
    const LongitudeLatitude geo{.LongitudeDeg = coordinates[at + 1],
                                .LatitudeDeg = coordinates[at]};
    if (!ValidAnchor(geo)) { return false; }
    const auto local = frame.ToLocalGroundPosition(geo);
    into.push_back({local.EastM, -local.NorthM});
  }
  return true;
}

std::expected<TerraceFootprint, std::string> Footprint(const PreparedStructure &source,
                                                       const PreparedStructureTile &tile,
                                                       const TangentFrame &frame,
                                                       std::vector<PolygonRing> &rings) {
  const auto &layout = source.Layout;
  if (layout.MinimumHeightM != 0) {
    return std::unexpected("elevated prepared footprint requires its detail representation");
  }
  if (layout.FirstHole > tile.Holes.size() ||
      layout.HoleCount > tile.Holes.size() - layout.FirstHole) {
    return std::unexpected("invalid prepared terrace hole range");
  }
  const GeographicRing shell{.First = layout.LocalFirst, .Count = layout.PointCount};
  const auto holes = std::span(tile.Holes).subspan(layout.FirstHole, layout.HoleCount);
  rings.resize(1 + holes.size());
  if (!ReadRing(tile.PointsLatLon, shell, frame, rings.front())) {
    return std::unexpected("invalid prepared terrace exterior");
  }
  for (size_t at = 0; at < holes.size(); ++at) {
    if (holes[at].Exterior || !ReadRing(tile.PointsLatLon, holes[at], frame, rings[at + 1])) {
      return std::unexpected("invalid prepared terrace hole");
    }
  }
  const size_t corner = static_cast<size_t>(shell.First) * 2;
  const LongitudeLatitudeHeight base{.LongitudeDeg = tile.PointsLatLon[corner + 1],
                                     .LatitudeDeg = tile.PointsLatLon[corner],
                                     .HeightM = source.BaseAslM};
  auto roof = base;
  roof.HeightM = source.SeatAslM + source.Standing.HeightM;
  return TerraceFootprint{.Rings = rings,
                          .BottomM = frame.ToLocalPosition(base).UpM,
                          .TopM = frame.ToLocalPosition(roof).UpM};
}

struct PreparedTerraces {
  UrbanPrisms Grid;
  TerraceRemainder Remainder;
};

std::expected<PreparedTerraces, std::string> Build(const PreparedStructureTile &tile,
                                                   const TerraceAssetRequest &request,
                                                   TerraceAssetBudget budget,
                                                   const std::stop_token &stop) {
  if (!tile.HeightQualified || tile.FallbackHeights || tile.Structures.empty() ||
      tile.PointsLatLon.size() % 2 != 0 ||
      tile.Structures.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected("prepared terrace elevations are unavailable");
  }
  const auto &ecef = tile.AnchorEcef;
  const auto centre = Ground::EcefToGeoWgs84({.X = ecef[0], .Y = ecef[1], .Z = ecef[2]});
  const LongitudeLatitude anchor{.LongitudeDeg = centre.LongitudeDeg,
                                 .LatitudeDeg = centre.LatitudeDeg};
  if (!ValidAnchor(anchor)) { return std::unexpected("invalid terrace tile anchor"); }
  const auto frame = TangentFrame::At(anchor);
  std::vector<std::vector<PolygonRing>> rings(tile.Structures.size());
  std::vector<TerraceFootprint> footprints;
  std::vector<uint32_t> ordinals;
  std::vector<uint32_t> retained;
  footprints.reserve(tile.Structures.size());
  for (uint32_t at = 0; at < tile.Structures.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected("terrace generation cancelled"); }
    const auto &structure = tile.Structures[at];
    const auto cell = structure.Layout.Cell.Index;
    if (cell == 0 || cell > std::numeric_limits<uint64_t>::digits) {
      return std::unexpected("invalid prepared terrace cell");
    }
    if ((request.Cells & (uint64_t{1} << (cell - 1))) == 0) { continue; }
    if (request.RetainRemainder && structure.Layout.MinimumHeightM != 0) {
      retained.push_back(at);
      continue;
    }
    auto footprint = Footprint(structure, tile, frame, rings[at]);
    if (!footprint) { return std::unexpected(footprint.error()); }
    footprints.push_back(*footprint);
    if (request.RetainRemainder) { ordinals.push_back(at); }
  }
  const TerraceRequest raster{.Anchor = anchor,
                              .SpacingM = request.SpacingM,
                              .SamplesMost = budget.SamplesMost,
                              .PointTestsMost = budget.PointTestsMost,
                              .Stop = stop,
                              .SupportMostM = request.SupportMostM};
  if (!request.RetainRemainder) {
    auto grid = BuildFootprintTerraces(footprints, raster);
    if (!grid) { return std::unexpected(grid.error()); }
    return PreparedTerraces{.Grid = std::move(*grid), .Remainder = {}};
  }
  auto partition = PartitionFootprintTerraces(footprints, raster);
  if (!partition) { return std::unexpected(partition.error()); }
  for (const size_t index : partition->Retained) { retained.push_back(ordinals[index]); }
  std::ranges::sort(retained);
  return PreparedTerraces{
      .Grid = std::move(partition->Grid),
      .Remainder = {.SourceCount = static_cast<uint32_t>(tile.Structures.size()),
                    .Sources = std::move(retained)}};
}

bool AppendRemainder(AssetPackage &package,
                     const TerraceRemainder &remainder,
                     const std::string &request,
                     size_t bytesMost) {
  BinaryValueWriter data(bytesMost - package.Bytes.size());
  if (!data(kRemainderFormat,
            remainder.SourceCount,
            static_cast<uint32_t>(remainder.Sources.size()))) {
    return false;
  }
  for (const uint32_t source : remainder.Sources) {
    if (!data(source)) { return false; }
  }
  const auto body = data.Out.Bytes();
  package.Records.push_back({.Key = RemainderKey(request),
                             .Kind = "building-terrace-remainder",
                             .Bounds = package.Records.front().Bounds,
                             .Package = {},
                             .OffsetBytes = package.Bytes.size(),
                             .ByteCount = body.size(),
                             .Parent = package.Records.front().Parent});
  package.Bytes.insert(package.Bytes.end(), body.begin(), body.end());
  return true;
}
}

std::expected<std::string, std::string> TerraceAssetRequest::Key() const {
  constexpr size_t keyChars = 64;
  constexpr size_t identityBytesMost = 128;
  if (BaseKey.size() != keyChars ||
      BaseKey.find_first_not_of("0123456789abcdef") != std::string::npos ||
      !ValidTerraceSpacing(SpacingM) || Cells == 0 || (RetainRemainder && SupportMostM == 0)) {
    return std::unexpected("invalid terrace asset identity");
  }
  BinaryValueWriter bytes(identityBytesMost);
  if (!bytes.Text("prepared-building-terraces-1") || !bytes.Text(BaseKey) ||
      !bytes(SpacingM, Cells) || (SupportMostM != 0 && !bytes(SupportMostM)) ||
      (RetainRemainder && !bytes(uint8_t{1}))) {
    return std::unexpected("could not encode terrace demand identity");
  }
  return Sha256Hex(bytes.Out.Bytes().data(), bytes.Out.Bytes().size());
}

std::expected<TerraceRemainder, std::string> ReadPreparedTerraceRemainder(
    const AssetCache &cache, const TerraceAssetRequest &request, size_t bytesMost) {
  const auto key = request.Key();
  if (!key || !request.RetainRemainder) {
    return std::unexpected("not a mixed building terrace identity");
  }
  const auto read = cache.Load(RemainderKey(*key), bytesMost);
  if (!read || !*read || (**read).Record().Parent != request.BaseKey ||
      (**read).Record().Kind != "building-terrace-remainder") {
    return std::unexpected("mixed terrace's source remainder is unavailable");
  }
  BinaryValueReader data((**read).Bytes());
  uint64_t format = 0;
  uint32_t count = 0;
  TerraceRemainder result;
  if (!data(format, result.SourceCount, count) || format != kRemainderFormat ||
      count >= result.SourceCount || uint64_t{count} * sizeof(uint32_t) != data.In.Remaining()) {
    return std::unexpected("invalid building terrace remainder header");
  }
  result.Sources.resize(count);
  for (auto &id : result.Sources) {
    if (!data(id) || id >= result.SourceCount) {
      return std::unexpected("invalid building terrace source ordinal");
    }
  }
  if (!std::ranges::is_sorted(result.Sources) ||
      std::ranges::adjacent_find(result.Sources) != result.Sources.end()) {
    return std::unexpected("building terrace remainder has duplicate or unordered sources");
  }
  return result;
}

std::expected<AssetPackageFactory, std::string>
CapturePreparedTerraces(std::shared_ptr<PreparedBuildingAssets> assets,
                        const TerraceAssetRequest &request,
                        TerraceAssetBudget budget,
                        const std::stop_token &stop) {
  if (!assets || budget.SamplesMost == 0 || budget.PointTestsMost == 0 || stop.stop_requested()) {
    return std::unexpected("terrace build is unavailable or has no work admission");
  }
  const auto key = request.Key();
  if (!key) { return std::unexpected(key.error()); }
  return AssetPackageFactory{[assets = std::move(assets), request, budget, stop](
                                 size_t bytesMost) -> std::expected<AssetPackage, std::string> {
    if (stop.stop_requested()) { return std::unexpected("terrace generation cancelled"); }
    const auto base = assets->Load(request.BaseKey);
    if (!base || !*base) { return std::unexpected("prepared terrace source is unavailable"); }
    return PackagePreparedTerraces(**base, request, budget, bytesMost, stop);
  }};
}

std::expected<AssetPackage, std::string>
PackagePreparedTerraces(const PreparedStructureTile &source,
                        const TerraceAssetRequest &request,
                        TerraceAssetBudget budget,
                        size_t bytesMost,
                        const std::stop_token &stop) {
  const auto key = request.Key();
  if (!key) { return std::unexpected(key.error()); }
  auto product = Build(source, request, budget, stop);
  if (!product) { return std::unexpected(product.error()); }
  if (product->Grid.Placements.empty()) {
    return std::unexpected("prepared source leaves no terrace coverage at this spacing");
  }
  if (stop.stop_requested()) { return std::unexpected("terrace generation cancelled"); }
  auto package =
      PackageUrbanPrisms(product->Grid, *key, {.Level = request.SpacingM, .BytesMost = bytesMost});
  if (!package) { return package; }
  package->Records.front().Parent = request.BaseKey;
  if (request.RetainRemainder && !AppendRemainder(*package, product->Remainder, *key, bytesMost)) {
    return std::unexpected("building terrace remainder exceeds the byte budget");
  }
  return package;
}
}
