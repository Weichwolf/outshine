#ifndef OUTSHINE_BASE_MATH_CLIPBOUNDS_H
#define OUTSHINE_BASE_MATH_CLIPBOUNDS_H

#include "Box.h"
#include "Mat4.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace outshine {

[[nodiscard]] constexpr double FloatRoundingMargin(double magnitude) {
  return 32 * std::numeric_limits<float>::epsilon() * (magnitude + 1);
}

[[nodiscard]] inline bool OutsideViewport(const Mat4f &clip, const Box &bounds) {
  if (bounds.Empty()) { return false; }
  for (size_t side = 0; side < 4; ++side) {
    const size_t row = side / 2;
    const double sign = side % 2 == 0 ? 1 : -1;
    double support = static_cast<double>(clip.At(3, 3)) + sign * clip.At(row, 3);
    double magnitude = std::abs(clip.At(3, 3)) + static_cast<double>(std::abs(clip.At(row, 3)));
    for (size_t axis = 0; axis < 3; ++axis) {
      const double a = clip.At(3, axis);
      const double b = clip.At(row, axis);
      const double coefficient = a + sign * b;
      const double low = bounds.Min[axis];
      const double high = bounds.Max[axis];
      if (!(low <= high)) { return false; }
      support += coefficient * (coefficient < 0 ? low : high);
      magnitude += (std::abs(a) + std::abs(b)) * std::max(std::abs(low), std::abs(high));
    }
    if (magnitude <= std::numeric_limits<float>::max() &&
        support < -FloatRoundingMargin(magnitude)) {
      return true;
    }
  }
  return false;
}

}
#endif
