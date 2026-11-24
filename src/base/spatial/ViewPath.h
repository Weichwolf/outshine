#ifndef OUTSHINE_BASE_SPATIAL_VIEWPATH_H
#define OUTSHINE_BASE_SPATIAL_VIEWPATH_H

#include "Vec3.h"
#include <algorithm>
#include <cmath>

namespace outshine {
struct ViewPath {
  Vec3 Start;
  Vec3 End;
  double RadiusM = 0;

  [[nodiscard]] double DistanceTo(const Vec3 &point) const noexcept {
    const Vec3 axis = End - Start;
    const Vec3 delta = point - Start;
    const double squared = Dot(axis, axis);
    const double along = squared > 0 ? std::clamp(Dot(delta, axis) / squared, 0.0, 1.0) : 0;
    return Length(delta - axis * along);
  }
};
}
#endif
