#ifndef OUTSHINE_BASE_SPATIAL_TRIANGLEDISTANCE_H
#define OUTSHINE_BASE_SPATIAL_TRIANGLEDISTANCE_H

#include <optional>
#include "math/Vec3.h"

namespace outshine {

[[nodiscard]] std::optional<double> BoundPointDistance(const Vec3 &pointM,
                                                       const Vec3 &targetM) noexcept;

struct TriangleDistanceBound {
  Vec3 TargetPointEstimateM;
  double LowerDistanceM = 0;
  double UpperDistanceM = 0;
};

[[nodiscard]] std::optional<TriangleDistanceBound> BoundPointTriangleDistance(
    const Vec3 &pointM, const Vec3 &aM, const Vec3 &bM, const Vec3 &cM) noexcept;

}
#endif
