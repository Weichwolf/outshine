#include "UrbanAreaTerrainPrisms.h"
#include "TerrainResidency.h"
#include "BlockedDigest.h"
#include "TileGeodesy.h"
#include "Units.h"
#include <Address.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine {
namespace {
constexpr int kIdentityZoom = 14;

std::expected<void, TerrainIdentityError> AppendRegions(const Generators::Osm::UrbanArea &area,
                                                        Generators::Osm::UrbanPatchRange patch,
                                                        std::vector<Data::TileId> &regions,
                                                        size_t most) {
  if (patch.Columns == 0 || patch.Rows == 0 || patch.Column >= area.Columns ||
      patch.Row >= area.Rows || patch.Columns > area.Columns - patch.Column ||
      patch.Rows > area.Rows - patch.Row) {
    return std::unexpected(TerrainIdentityError::InvalidRequest);
  }
  const double pitch = area.Recipe.ProjectedPitchM;
  const double west = (static_cast<double>(area.FirstCell[0]) + patch.Column + .5) * pitch;
  const double east =
      (static_cast<double>(area.FirstCell[0]) + patch.Column + patch.Columns - .5) * pitch;
  const double north = (static_cast<double>(area.FirstCell[1]) - patch.Row + .5) * pitch;
  const double south =
      (static_cast<double>(area.FirstCell[1]) - patch.Row - patch.Rows + 1.5) * pitch;
  const auto at = [](double x, double y) {
    return Ground::TileIndex::Of({.LongitudeDeg = x / kWgs84A * kRad2Deg,
                                  .LatitudeDeg = std::atan(std::sinh(y / kWgs84A)) * kRad2Deg},
                                 kIdentityZoom)
        .Tile();
  };
  const auto first = at(west, north);
  const auto last = at(east, south);
  if (!first || !last || first->X > last->X || first->Y > last->Y) {
    return std::unexpected(TerrainIdentityError::InvalidRequest);
  }
  for (uint32_t row = first->Y; row <= last->Y; ++row) {
    for (uint32_t column = first->X; column <= last->X; ++column) {
      const Data::TileId region{.Zoom = kIdentityZoom, .X = column, .Y = row};
      if (std::ranges::find(regions, region) != regions.end()) { continue; }
      if (regions.size() == most) { return std::unexpected(TerrainIdentityError::OverBudget); }
      regions.push_back(region);
    }
  }
  return {};
}
}

std::expected<std::array<uint8_t, 32>, TerrainIdentityError>
UrbanPrismTerrainIdentity(const Generators::Osm::UrbanArea &area,
                          std::span<const Generators::Osm::UrbanPatchRange> patches,
                          const TerrainResidency &terrain,
                          UrbanTerrainIdentityLimits limits) {
  if (!area.Recipe.Valid() || patches.empty()) {
    return std::unexpected(TerrainIdentityError::InvalidRequest);
  }
  if (patches.size() > limits.PatchesMost) {
    return std::unexpected(TerrainIdentityError::OverBudget);
  }
  std::vector<Data::TileId> regions;
  for (const auto patch : patches) {
    const auto appended = AppendRegions(area, patch, regions, limits.RegionsMost);
    if (!appended) { return std::unexpected(appended.error()); }
  }
  std::ranges::sort(regions, [](Data::TileId left, Data::TileId right) {
    return left.Y == right.Y ? left.X < right.X : left.Y < right.Y;
  });
  BlockedDigest digest;
  constexpr uint32_t format = 1;
  (void)digest.Number(format);
  (void)digest.Number(kIdentityZoom);
  (void)digest.Number(static_cast<uint64_t>(regions.size()));
  for (const auto region : regions) {
    const auto identity = terrain.SurfaceIdentity(region, limits.PagesMost);
    if (!identity) { return std::unexpected(identity.error()); }
    (void)digest.Number(region.X);
    (void)digest.Number(region.Y);
    (void)digest.Put(*identity);
  }
  return digest.Finish();
}
}
