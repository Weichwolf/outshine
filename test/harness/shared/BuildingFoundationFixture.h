#ifndef OUTSHINE_TEST_BUILDINGFOUNDATIONFIXTURE_H
#define OUTSHINE_TEST_BUILDINGFOUNDATIONFIXTURE_H
#include "BuildingPhysicalProduct.h"
#include "BuildingFootprint.h"
#include "BuildingGeometry.h"
#include "GeographicRing.h"
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Test {
[[nodiscard]] inline std::vector<Ground::BuildingProductLease>
FoundationProducts(std::span<const Ground::BuildingFootprint> prints,
                   std::span<const double> points,
                   std::span<const GeographicRing> rings = {}) {
  auto physical = std::make_shared<Ground::BuildingPhysicalProduct>();
  physical->Prints.assign(prints.begin(), prints.end());
  for (const auto &print : prints) { physical->ContactBytes += print.HeapBytes(); }
  auto coordinates = std::make_shared<Ground::BuildingGeometry>();
  coordinates->OwnCoordinates({points.begin(), points.end()}, {rings.begin(), rings.end()});
  return {{.Physical = std::move(physical),
           .Coordinates = std::move(coordinates),
           .FirstFootprint = 0}};
}
}
#endif
