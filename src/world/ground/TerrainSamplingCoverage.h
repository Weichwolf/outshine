#ifndef OUTSHINE_WORLD_GROUND_TERRAINSAMPLINGCOVERAGE_H
#define OUTSHINE_WORLD_GROUND_TERRAINSAMPLINGCOVERAGE_H

#include <cstdint>
#include <optional>

#include <world/data/Address.h>

namespace outshine::Ground {

struct TerrainSamplingCoverage {
  static constexpr int FallbackLevels = 3;
  Data::TileId Fine;
  std::optional<Data::TileId> Coarse;

  [[nodiscard]] static constexpr std::optional<TerrainSamplingCoverage>
  ForField(Data::TileId field) noexcept;
};

constexpr std::optional<TerrainSamplingCoverage>
TerrainSamplingCoverage::ForField(Data::TileId field) noexcept {
  if (field.Zoom < 0 || field.Zoom > Data::TileId::MaximumZoom) { return std::nullopt; }
  const uint32_t side = uint32_t{1} << static_cast<uint32_t>(field.Zoom);
  if (field.X >= side || field.Y >= side) { return std::nullopt; }
  TerrainSamplingCoverage coverage{.Fine = field, .Coarse = std::nullopt};
  if (field.Zoom > FallbackLevels) {
    coverage.Coarse = Data::TileId{.Zoom = field.Zoom - FallbackLevels,
                                   .X = field.X >> static_cast<uint32_t>(FallbackLevels),
                                   .Y = field.Y >> static_cast<uint32_t>(FallbackLevels)};
  }
  return coverage;
}
}

#endif
