#include "Wayfinding.h"
#include "Geodesy.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <numeric>

namespace outshine::Path {
Box Network::BoundsEcef() const {
  Box bounds;
  if (Points_.empty()) { return bounds; }
  double minLat = Points_[0];
  double maxLat = minLat;
  double minLon = Points_[1];
  double maxLon = minLon;
  double lowM = 0;
  double highM = 0;
  double halfWidthM = 0;
  for (size_t point = 0; point < PointCount(); ++point) {
    minLat = std::min(minLat, Points_[2 * point]);
    maxLat = std::max(maxLat, Points_[2 * point]);
    minLon = std::min(minLon, Points_[2 * point + 1]);
    maxLon = std::max(maxLon, Points_[2 * point + 1]);
  }
  for (const double height : HeightsM_) {
    lowM = std::min(lowM, height);
    highM = std::max(highM, height);
  }
  for (const auto &way : Ways_) { halfWidthM = std::max(halfWidthM, way.HalfWidthM); }
  Vec3 centre;
  GeoToEcef({.LongitudeDeg = std::midpoint(minLon, maxLon),
             .LatitudeDeg = std::midpoint(minLat, maxLat),
             .HeightM = std::midpoint(lowM, highM)},
            centre);
  constexpr double derivativeBoundM = 6400000;
  const double radius = (derivativeBoundM + std::max(std::abs(lowM), std::abs(highM))) *
                            (maxLon - minLon + maxLat - minLat) * std::numbers::pi / kDegPerTurn +
                        (highM - lowM) * 0.5 + halfWidthM;
  for (size_t axis = 0; axis < 3; ++axis) {
    bounds.Min[axis] = centre[axis] - radius;
    bounds.Max[axis] = centre[axis] + radius;
  }
  return bounds;
}
}
