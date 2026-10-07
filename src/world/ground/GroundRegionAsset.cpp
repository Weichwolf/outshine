#include "GroundRegionAsset.h"
#include "BinaryValueArchive.h"
#include "math/Units.h"
#include "content/GeometryAsset.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace outshine {
namespace {
constexpr uint64_t kFormat = 0x0002314e5247ULL;
constexpr uint32_t kPagesMost = 65536;
constexpr size_t kPageHeaderBytes = 7 * sizeof(uint32_t) + sizeof(uint8_t);

template <class T> bool Finite(std::span<const T> values) {
  return std::ranges::all_of(values, [](T value) { return std::isfinite(value); });
}

bool ValidGrid(const ClassStructure::Grid &grid) {
  if (grid.W < 0 || grid.H < 0 || !std::isfinite(grid.OrgE) || !std::isfinite(grid.OrgN) ||
      !std::isfinite(grid.CellM) || grid.CellM <= 0 || grid.Seeds.size() % 3 != 0 ||
      grid.Edges.size() % 4 != 0 ||
      grid.Cells.size() != static_cast<uint64_t>(grid.W) * static_cast<uint64_t>(grid.H) * 2 ||
      !Finite<float>(grid.Edges)) {
    return false;
  }
  for (size_t cell = 0; cell < grid.Cells.size(); cell += 2) {
    const size_t count = (grid.Cells[cell] >> 16u) & 255u;
    const size_t first = grid.Cells[cell + 1];
    if (first > grid.Seeds.size() / 3 || count > grid.Seeds.size() / 3 - first) { return false; }
  }
  for (size_t seed = 0; seed < grid.Seeds.size(); seed += 3) {
    const size_t count = (grid.Seeds[seed] >> 16u) & 255u;
    const size_t first = grid.Seeds[seed + 1];
    if (first > grid.Refs.size() || count > grid.Refs.size() - first ||
        !std::isfinite(std::bit_cast<float>(grid.Seeds[seed + 2]))) {
      return false;
    }
  }
  return std::ranges::all_of(grid.Refs,
                             [&](uint32_t edge) { return edge < grid.Edges.size() / 4; });
}

template <class Archive, class Grid> bool GridFields(Archive &archive, Grid &grid) {
  return archive(grid.W, grid.H, grid.OrgE, grid.OrgN, grid.CellM) &&
         archive.template Array<uint32_t>(grid.Cells) &&
         archive.template Array<uint32_t>(grid.Seeds) &&
         archive.template Array<uint32_t>(grid.Refs) && archive.template Array<float>(grid.Edges) &&
         ValidGrid(grid);
}

template <class Archive, class Page> bool PageFields(Archive &archive, Page &page) {
  if (!archive(page.Tile.Zoom,
               page.Tile.X,
               page.Tile.Y,
               page.Side,
               page.Postings,
               page.Virtual,
               page.SourceZoom) ||
      !archive.template Array<float>(page.Nodes) || page.Tile.Zoom < 0 ||
      page.Tile.Zoom > Data::TileId::MaximumZoom ||
      page.Tile.X >= (uint32_t{1} << static_cast<unsigned>(page.Tile.Zoom)) ||
      page.Tile.Y >= (uint32_t{1} << static_cast<unsigned>(page.Tile.Zoom)) || page.Side < 1 ||
      page.Side > 4096 || page.Postings < 2 || page.SourceZoom < -1 ||
      page.SourceZoom > page.Tile.Zoom || !Finite<float>(page.Nodes)) {
    return false;
  }
  const auto side = static_cast<size_t>(page.Side);
  return page.Nodes.size() == side * side || page.Nodes.size() == (side + 2) * (side + 2);
}

template <class Archive, class Patch> bool PatchFields(Archive &archive, Patch &terrain) {
  std::conditional_t<std::is_const_v<Patch>, const std::array<uint64_t, 8>, std::array<uint64_t, 8>>
      counts = {terrain.Tiles,
                terrain.Pending,
                terrain.Absent,
                terrain.Refused,
                terrain.Skipped,
                terrain.Bare,
                terrain.ContactPending,
                terrain.Overlapped};
  std::conditional_t<std::is_const_v<Patch>, const int64_t, int64_t> reachTiles =
      terrain.ReachTiles;
  if (!archive(
          counts, reachTiles, terrain.CoarsestZoom, terrain.PendingAtZoom, terrain.WantedAtZoom)) {
    return false;
  }
  if constexpr (!std::is_const_v<Patch>) {
    if (reachTiles < std::numeric_limits<long>::min() ||
        reachTiles > std::numeric_limits<long>::max() ||
        !std::ranges::all_of(
            counts, [](uint64_t value) { return value <= std::numeric_limits<size_t>::max(); })) {
      return false;
    }
    const std::array<size_t *, 8> targets = {&terrain.Tiles,
                                             &terrain.Pending,
                                             &terrain.Absent,
                                             &terrain.Refused,
                                             &terrain.Skipped,
                                             &terrain.Bare,
                                             &terrain.ContactPending,
                                             &terrain.Overlapped};
    for (size_t at = 0; at < counts.size(); ++at) {
      *targets[at] = static_cast<size_t>(counts[at]);
    }
    terrain.ReachTiles = static_cast<long>(reachTiles);
  }
  return true;
}

template <class Archive, class Region> bool RegionFields(Archive &archive, Region &region) {
  return archive(region.Anchor.LongitudeDeg,
                 region.Anchor.LatitudeDeg,
                 region.NetworkWays,
                 region.GroundSurface,
                 region.MissingRims,
                 region.GroundAlbedo[0],
                 region.GroundAlbedo[1],
                 region.GroundAlbedo[2]) &&
         PatchFields(archive, region.Terrain) && archive.template Array<float>(region.ClassPalette);
}

bool ValidRegion(const GroundRegionAsset &region) {
  return std::isfinite(region.Anchor.LongitudeDeg) && std::isfinite(region.Anchor.LatitudeDeg) &&
         std::abs(region.Anchor.LongitudeDeg) <= kDegPerHalfTurn &&
         std::abs(region.Anchor.LatitudeDeg) <= (kDegPerHalfTurn * 0.5) &&
         region.GroundSurface >= 0 && region.GroundSurface < region.Surfaces.surfaces() &&
         std::ranges::all_of(
             region.GroundAlbedo,
             [](double value) { return std::isfinite(value) && value >= 0 && value <= 1; });
}

bool WriteClasses(BinaryValueWriter &out, const std::shared_ptr<const ClassStructure> &classes) {
  if (!out(classes != nullptr)) { return false; }
  if (!classes) { return true; }
  const auto anchor = classes->Frame().Anchor();
  return out(anchor.LongitudeDeg,
             anchor.LatitudeDeg,
             classes->Version(),
             classes->UnmappedRow(),
             classes->Measured().Overflow) &&
         GridFields(out, classes->Fine()) && GridFields(out, classes->Coarse());
}

bool ReadClasses(BinaryValueReader &in, std::shared_ptr<const ClassStructure> &classes) {
  bool present = false;
  if (!in(present)) { return false; }
  if (!present) { return true; }
  LongitudeLatitude anchor;
  ClassStructure::FromRun version;
  auto fine = std::make_shared<ClassStructure::Grid>();
  auto coarse = std::make_shared<ClassStructure::Grid>();
  if (!in(anchor.LongitudeDeg,
          anchor.LatitudeDeg,
          version.Version,
          version.UnmappedRow,
          version.Overflow) ||
      !std::isfinite(anchor.LongitudeDeg) || !std::isfinite(anchor.LatitudeDeg) ||
      std::abs(anchor.LongitudeDeg) > kDegPerHalfTurn ||
      std::abs(anchor.LatitudeDeg) > (kDegPerHalfTurn * 0.5) || !GridFields(in, *fine) ||
      !GridFields(in, *coarse)) {
    return false;
  }
  classes = std::make_shared<const ClassStructure>(
      TangentFrame::At(anchor), std::move(fine), std::move(coarse), version);
  return true;
}
}

std::optional<std::vector<uint8_t>> EncodeGroundRegionAsset(const GroundRegionAsset &region,
                                                            size_t bytesMost) {
  if (!ValidRegion(region) || region.Terrain.Sheets.size() > kPagesMost) { return std::nullopt; }
  BinaryValueWriter out(bytesMost);
  if (!out(kFormat) || !RegionFields(out, region) ||
      !out(static_cast<uint32_t>(region.Terrain.Sheets.size())) ||
      !std::ranges::all_of(region.Terrain.Sheets,
                           [&](const Sheet &page) { return PageFields(out, page); }) ||
      !WriteClasses(out, region.Classes) || !out(region.Network != nullptr)) {
    return std::nullopt;
  }
  if (region.Network) {
    const auto bytes = region.Network->EncodeAsset(bytesMost - out.Out.Bytes().size());
    if (!bytes || !out.Array<uint8_t>(*bytes)) { return std::nullopt; }
  }
  const auto geometry = EncodeGeometryAsset(region.Surfaces, bytesMost - out.Out.Bytes().size());
  if (!geometry || !out.Out.Put(*geometry)) { return std::nullopt; }
  return std::move(out.Out).TakeBytes();
}

std::optional<GroundRegionAsset> DecodeGroundRegionAsset(std::span<const uint8_t> bytes,
                                                         size_t bytesMost) {
  if (bytes.size() > bytesMost) { return std::nullopt; }
  BinaryValueReader in(bytes);
  uint64_t format = 0;
  uint32_t pages = 0;
  GroundRegionAsset region;
  if (!in(format) || format != kFormat || !RegionFields(in, region) || !in(pages) ||
      pages > kPagesMost || pages > in.In.Remaining() / kPageHeaderBytes) {
    return std::nullopt;
  }
  region.Terrain.Sheets.resize(pages);
  if (!std::ranges::all_of(region.Terrain.Sheets,
                           [&](Sheet &page) { return PageFields(in, page); }) ||
      !ReadClasses(in, region.Classes)) {
    return std::nullopt;
  }
  bool network = false;
  if (!in(network)) { return std::nullopt; }
  if (network) {
    std::vector<uint8_t> encoded;
    if (!in.Array(encoded)) { return std::nullopt; }
    auto graph = Path::Network::DecodeAsset(encoded, bytesMost);
    if (!graph) { return std::nullopt; }
    region.Network = std::make_shared<const Path::Network>(std::move(*graph));
  }
  const auto geometry = in.In.Take(in.In.Remaining());
  if (!geometry) { return std::nullopt; }
  auto decoded = DecodeGeometryAsset(*geometry, bytesMost);
  if (!decoded) { return std::nullopt; }
  region.Surfaces = std::move(*decoded);
  if (!ValidRegion(region)) { return std::nullopt; }
  return region;
}
}
