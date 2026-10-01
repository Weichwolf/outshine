#ifndef OUTSHINE_WORLD_PRODUCTS_BUILDINGGEOMETRY_H
#define OUTSHINE_WORLD_PRODUCTS_BUILDINGGEOMETRY_H

#include "Capacity.h"
#include "GeographicRing.h"
#include "SourceProvenance.h"
#include <vector>

namespace outshine::Ground {

struct BuildingGeometry {
  Data::ProductOrigin Origin;
  std::vector<double> Points;
  std::vector<GeographicRing> Rings;
  std::vector<Data::SourceObjectId> Sources;

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return CapacityBytes(Points) + CapacityBytes(Rings) + CapacityBytes(Sources);
  }
};

}

#endif
