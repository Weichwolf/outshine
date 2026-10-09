#include "BuildingWallNormals.h"
#include <cstddef>
#include <cmath>
#include <algorithm>

namespace outshine::Generators {
bool HasCurvedShaftWalls(const BuildingShape &shape) noexcept {
  constexpr size_t leastCorners = 8;
  const bool closed = shape.OpeningStyle == FacadeStyle::Tower ||
                      shape.OpeningStyle == FacadeStyle::Outbuilding ||
                      shape.OpeningStyle == FacadeStyle::Hall;
  return shape.Form == BuildingForm::Tower && closed && shape.Holes.empty() &&
         shape.RoundFootprint && shape.Ring.size() >= leastCorners;
}

Vec3 BuildingWallShadingNormal(const BuildingShape &shape,
                               const EastNorth &point,
                               const Vec3 &geometricNormal) noexcept {
  constexpr double wallUpTolerance = 1e-6;
  if (!HasCurvedShaftWalls(shape) || std::abs(geometricNormal[2]) > wallUpTolerance) {
    return geometricNormal;
  }
  const auto at = shape.ToBox(point);
  const double u = at.U / (shape.HalfUm * shape.HalfUm);
  const double v = at.V / (shape.HalfVm * shape.HalfVm);
  const auto axisV = shape.AxisV();
  const double east = u * shape.AxisU.EastM + v * axisV.EastM;
  const double north = u * shape.AxisU.NorthM + v * axisV.NorthM;
  const double length = std::hypot(east, north);
  return length > 0.0 ? Vec3{{east / length, north / length, 0.0}} : geometricNormal;
}

Vec3 BuildingWallInterpolatedNormal(const BuildingShape &shape,
                                    size_t edge,
                                    const EastNorth &point,
                                    const Vec3 &geometricNormal) noexcept {
  if (!HasCurvedShaftWalls(shape)) { return geometricNormal; }
  const auto &p = shape.Ring[edge];
  const auto &q = shape.Ring[(edge + 1) % shape.Ring.size()];
  const double east = q.EastM - p.EastM;
  const double north = q.NorthM - p.NorthM;
  const double lengthSquared = east * east + north * north;
  if (lengthSquared == 0.0) { return geometricNormal; }
  const double fraction = std::clamp(
      ((point.EastM - p.EastM) * east + (point.NorthM - p.NorthM) * north) / lengthSquared,
      0.0,
      1.0);
  const Vec3 normal = BuildingWallShadingNormal(shape, p, geometricNormal) * (1.0 - fraction) +
                      BuildingWallShadingNormal(shape, q, geometricNormal) * fraction;
  const double length = std::sqrt(Dot(normal, normal));
  return length > 0.0 ? normal * (1.0 / length) : geometricNormal;
}
}
