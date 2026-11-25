#include "PreparedTerraceAsset.h"
#include "BinaryValueArchive.h"
#include "FootprintTerraces.h"
#include "Geodesy.h"
#include "PreparedBuildingAssets.h"
#include "Sha256.h"
#include "TangentFrame.h"
#include "TileGeodesy.h"
#include "UrbanPrismAsset.h"
#include "Units.h"
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

std::expected<UrbanPrisms, std::string> Build(const PreparedStructureTile &tile,
                                              const TerraceAssetRequest &request,
                                              TerraceAssetBudget budget,
                                              const std::stop_token &stop) {
  if (!tile.HeightQualified || tile.FallbackHeights || tile.Structures.empty() ||
      tile.PointsLatLon.size() % 2 != 0) {
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
  footprints.reserve(tile.Structures.size());
  for (size_t at = 0; at < tile.Structures.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected("terrace generation cancelled"); }
    const auto &structure = tile.Structures[at];
    const auto cell = structure.Layout.Cell.Index;
    if (cell == 0 || cell > std::numeric_limits<uint64_t>::digits) {
      return std::unexpected("invalid prepared terrace cell");
    }
    if ((request.Cells & (uint64_t{1} << (cell - 1))) == 0) { continue; }
    auto footprint = Footprint(structure, tile, frame, rings[at]);
    if (!footprint) { return std::unexpected(footprint.error()); }
    footprints.push_back(*footprint);
  }
  return BuildFootprintTerraces(footprints,
                                {.Anchor = anchor,
                                 .SpacingM = request.SpacingM,
                                 .SamplesMost = budget.SamplesMost,
                                 .PointTestsMost = budget.PointTestsMost,
                                 .Stop = stop,
                                 .SupportMostM = request.SupportMostM});
}
}

std::expected<std::string, std::string> TerraceAssetRequest::Key() const {
  constexpr size_t keyChars = 64;
  constexpr size_t identityBytesMost = 128;
  if (BaseKey.size() != keyChars ||
      BaseKey.find_first_not_of("0123456789abcdef") != std::string::npos ||
      !ValidTerraceSpacing(SpacingM) || Cells == 0) {
    return std::unexpected("invalid terrace asset identity");
  }
  BinaryValueWriter bytes(identityBytesMost);
  if (!bytes.Text("prepared-building-terraces-1") || !bytes.Text(BaseKey) ||
      !bytes(SpacingM, Cells) || (SupportMostM != 0 && !bytes(SupportMostM))) {
    return std::unexpected("could not encode terrace demand identity");
  }
  return Sha256Hex(bytes.Out.Bytes().data(), bytes.Out.Bytes().size());
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
  if (product->Placements.empty()) {
    return std::unexpected("prepared source leaves no terrace coverage at this spacing");
  }
  if (stop.stop_requested()) { return std::unexpected("terrace generation cancelled"); }
  auto package =
      PackageUrbanPrisms(*product, *key, {.Level = request.SpacingM, .BytesMost = bytesMost});
  if (package) { package->Records.front().Parent = request.BaseKey; }
  return package;
}
}
