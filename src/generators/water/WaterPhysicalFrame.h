#ifndef OUTSHINE_GENERATORS_WATER_WATERPHYSICALFRAME_H
#define OUTSHINE_GENERATORS_WATER_WATERPHYSICALFRAME_H

#include "TangentFrame.h"
#include "Vec3.h"
#include <array>
#include <expected>

namespace outshine::Generators {
enum class WaterFrameError { InvalidInput, OutsideLocalDomain, NoConvergence };

struct WaterGeodeticPoint {
  LongitudeLatitudeHeight Geographic;
  Vec3 Up;
};

struct WaterNormalCap {
  Vec3 Center;
  Vec3 Up;
  double RadiusM = 0;
  double Delta = 0;
};

[[nodiscard]] std::expected<WaterGeodeticPoint, WaterFrameError>
GeodeticWaterPoint(const TangentFrame &frame, const Vec3 &point);

[[nodiscard]] std::expected<double, WaterFrameError>
WaterHeightTarget(const TangentFrame &frame, EastNorth point, double modelHeightM);

[[nodiscard]] std::expected<WaterNormalCap, WaterFrameError>
BoundWaterFacetNormals(const TangentFrame &frame,
                       const std::array<std::array<double, 2>, 3> &triangle,
                       std::array<double, 2> upBoundsM);
}
#endif
