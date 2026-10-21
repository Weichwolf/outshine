#ifndef OUTSHINE_WORLD_PRODUCTS_BUILDINGGEOMETRY_H
#define OUTSHINE_WORLD_PRODUCTS_BUILDINGGEOMETRY_H

#include "Capacity.h"
#include "BuildingCoordinates.h"
#include "SourceProvenance.h"
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Ground {
struct BuildingGeometry {
  Data::ProductOrigin Origin;
  BuildingCoordinates Coordinates;
  std::vector<Data::SourceObjectId> Sources;

  void OwnCoordinates(std::vector<double> points, std::vector<GeographicRing> rings) {
    Coordinates.Own(std::move(points), std::move(rings));
  }

  void ShareCoordinates(std::shared_ptr<const void> owner,
                        std::span<const double> points,
                        std::span<const GeographicRing> rings,
                        size_t retainedBytes) {
    Coordinates.Share(std::move(owner), points, rings, retainedBytes);
  }

  [[nodiscard]] std::span<const double> Points() const noexcept { return Coordinates.Points(); }

  [[nodiscard]] std::span<const GeographicRing> Rings() const noexcept {
    return Coordinates.Rings();
  }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return Coordinates.HeapBytes() + CapacityBytes(Sources);
  }
};
}

#endif
