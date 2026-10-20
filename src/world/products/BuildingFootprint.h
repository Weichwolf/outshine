#ifndef OUTSHINE_WORLD_PRODUCTS_BUILDINGFOOTPRINT_H
#define OUTSHINE_WORLD_PRODUCTS_BUILDINGFOOTPRINT_H

#include "BuildingFrontage.h"
#include <cstdint>
#include <optional>

namespace outshine::Ground {

enum class BuildingHeightSource : uint8_t { Declared, Generated };

struct BuildingTerrainCeiling {
  uint32_t OriginPoint = 0;
  double HeightAslM = 0.0;

  [[nodiscard]] bool operator==(const BuildingTerrainCeiling &) const noexcept = default;
};

struct BuildingFootprint {
  uint32_t FirstPoint = 0, PointCount = 0;
  uint32_t FirstHole = 0, HoleCount = 0;
  float HeightM = 0.0f;
  float MinimumHeightM = 0.0f;
  float BaseM = 0.0f;
  float SeatM = 0.0f;
  float FootM = 0.0f;
  BuildingHeightSource Source = BuildingHeightSource::Generated;
  BuildingFrontage Street;
  std::optional<BuildingTerrainCeiling> TerrainCeiling;

  [[nodiscard]] bool operator==(const BuildingFootprint &) const noexcept = default;
};

}

#endif
