#ifndef OUTSHINE_WORLD_PRODUCTS_BUILDINGTERRAINCEILING_H
#define OUTSHINE_WORLD_PRODUCTS_BUILDINGTERRAINCEILING_H

#include "Capacity.h"
#include "Earth.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace outshine::Ground {

struct BuildingTerrainPart {
  double HeightAslM = 0.0;
  std::vector<EastNorth> Ring;
  std::vector<std::vector<EastNorth>> Holes;

  [[nodiscard]] bool operator==(const BuildingTerrainPart &) const = default;

  [[nodiscard]] bool Valid() const noexcept {
    const auto ring = [](const auto &points) {
      return points.size() >= 3 && std::ranges::all_of(points, [](const auto &point) {
               return std::isfinite(point.EastM) && std::isfinite(point.NorthM);
             });
    };
    return std::isfinite(HeightAslM) && ring(Ring) && std::ranges::all_of(Holes, ring);
  }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    size_t bytes = CapacityBytes(Ring) + CapacityBytes(Holes);
    for (const auto &hole : Holes) { bytes += CapacityBytes(hole); }
    return bytes;
  }
};

struct BuildingTerrainCeiling {
  uint32_t OriginPoint = 0;
  double HeightAslM = 0.0;

  [[nodiscard]] bool operator==(const BuildingTerrainCeiling &) const noexcept = default;
};

struct BuildingTerrainContacts {
  std::shared_ptr<const std::vector<BuildingTerrainPart>> Parts;

  [[nodiscard]] std::span<const BuildingTerrainPart> PartsView() const noexcept {
    return Parts ? std::span<const BuildingTerrainPart>(*Parts)
                 : std::span<const BuildingTerrainPart>{};
  }

  [[nodiscard]] bool operator==(const BuildingTerrainContacts &other) const noexcept {
    return Parts == other.Parts || std::ranges::equal(PartsView(), other.PartsView());
  }

  [[nodiscard]] bool Valid() const noexcept {
    return std::ranges::all_of(PartsView(), [](const auto &part) { return part.Valid(); });
  }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    if (!Parts) { return 0; }
    size_t bytes = sizeof(std::vector<BuildingTerrainPart>) + CapacityBytes(*Parts);
    for (const auto &part : *Parts) { bytes += part.HeapBytes(); }
    return bytes;
  }
};

}
#endif
