#ifndef OUTSHINE_GENERATORS_OSM_FIELDS_WATERBOUNDARYINTERVALS_H
#define OUTSHINE_GENERATORS_OSM_FIELDS_WATERBOUNDARYINTERVALS_H

#include "WaterField.h"

#include <limits>
#include <span>
#include <vector>

namespace outshine::Generators::Osm {
enum class WaterBoundaryAxis { X, Y };

struct WaterBoundaryInterval {
  double From = 0.0, To = 0.0;
};

struct WaterBoundaryCut {
  WaterBoundaryAxis Axis = WaterBoundaryAxis::X;
  double Coordinate = 0.0;
  double From = 0.0, To = 0.0;
};

inline constexpr double kWaterBoundaryTolerance = 64.0 * std::numeric_limits<double>::epsilon();

[[nodiscard]] std::vector<WaterBoundaryInterval> WaterBoundaryIntervals(
    const OsmField &field, std::span<const WaterField::SurfaceRing> rings, WaterBoundaryCut cut);

[[nodiscard]] bool WaterBoundariesOverlap(std::span<const WaterBoundaryInterval> first,
                                          std::span<const WaterBoundaryInterval> second);
}
#endif
