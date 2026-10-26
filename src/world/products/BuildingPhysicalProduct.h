#ifndef OUTSHINE_WORLD_PRODUCTS_BUILDINGPHYSICALPRODUCT_H
#define OUTSHINE_WORLD_PRODUCTS_BUILDINGPHYSICALPRODUCT_H

#include "BuildingFootprint.h"
#include "BuildingGeometry.h"
#include <cstddef>
#include <memory>
#include <vector>

namespace outshine::Ground {

struct BuildingPhysicalProduct {
  std::vector<BuildingFootprint> Prints;
  std::vector<double> SeatSpreadM;
  std::vector<double> AcrossM;
  size_t ContactBytes = 0;

  [[nodiscard]] size_t HeapBytes() const noexcept;
};

struct BuildingProductLease {
  std::shared_ptr<const BuildingPhysicalProduct> Physical;
  std::shared_ptr<const BuildingGeometry> Coordinates;
  size_t FirstFootprint = 0;
};

}
#endif
