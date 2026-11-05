#include "ShadowDemand.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace outshine::Render {
bool ShadowDemand::MayContain(const Box &bounds) const noexcept {
  if (Regions == 0) { return false; }
  if (Regions > ClipFromScene.size() || bounds.Empty()) { return true; }
  double magnitude = 1;
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(bounds.Min[axis]) || !std::isfinite(bounds.Max[axis]) ||
        bounds.Min[axis] > bounds.Max[axis] || !std::isfinite(EyeM[axis])) {
      return true;
    }
    magnitude += std::abs(bounds.Min[axis]) + std::abs(bounds.Max[axis]) + std::abs(EyeM[axis]);
  }
  const double errorM = 64 * std::numeric_limits<float>::epsilon() * magnitude;
  constexpr std::array minimumClip{-1., -1., 0.};
  for (size_t region = 0; region < Regions; ++region) {
    const auto &matrix = ClipFromScene[region];
    if (!std::ranges::all_of(matrix, [](double v) { return std::isfinite(v); }) || matrix[3] != 0 ||
        matrix[7] != 0 || matrix[11] != 0 || matrix[15] != 1) {
      return true;
    }
    bool outside = false;
    for (size_t row = 0; row < 3; ++row) {
      double least = matrix[12 + row];
      double most = least;
      double guard = 64 * std::numeric_limits<float>::epsilon() * (1 + std::abs(least));
      for (size_t axis = 0; axis < 3; ++axis) {
        const double a = matrix[axis * 4 + row] * bounds.Min[axis];
        const double b = matrix[axis * 4 + row] * bounds.Max[axis];
        least += std::min(a, b);
        most += std::max(a, b);
        guard += std::abs(matrix[axis * 4 + row]) * errorM;
      }
      if (!std::isfinite(least) || !std::isfinite(most) || !std::isfinite(guard)) { return true; }
      outside |= most < minimumClip[row] - guard || least > 1 + guard;
    }
    if (!outside) { return true; }
  }
  return false;
}
}
