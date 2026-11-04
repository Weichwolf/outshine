#include "ClipmapVisibility.h"
#include "ClipmapHeightCache.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace outshine::Render {
namespace {
constexpr double kGuard = 64 * std::numeric_limits<float>::epsilon();

struct Volume {
  std::array<double, 3> Low{}, High{};
};

Volume Bounds(const ClipmapBlock &block,
              const ClipmapRing &ring,
              std::array<float, 2> heights,
              float heightShift) {
  Volume result;
  const double step = ring.CentreStep[2];
  for (size_t axis = 0; axis < 2; ++axis) {
    const double centre = ring.RelativeCentres[axis];
    const double a = centre + block.Low[axis] * step;
    const double b = centre + block.High[axis] * step;
    const double padding =
        kGuard *
        (std::abs(centre) +
         std::max(std::abs(block.Low[axis]), std::abs(block.High[axis])) * std::abs(step) + 1);
    const size_t coordinate = axis == 0 ? 0 : 2;
    result.Low[coordinate] = (axis == 0 ? a : -b) - padding;
    result.High[coordinate] = (axis == 0 ? b : -a) + padding;
  }
  const double padding =
      kGuard * (std::max(std::abs(heights[0]), std::abs(heights[1])) + std::abs(heightShift) + 1);
  result.Low[1] = static_cast<double>(heights[0]) + heightShift - padding;
  result.High[1] = static_cast<double>(heights[1]) + heightShift + padding;
  return result;
}
}

std::array<float, 2> ClipmapHeightRange(const ClipmapHeightCache &heights,
                                        uint32_t level) noexcept {
  std::array<float, 2> result{std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()};
  const auto end = std::min(uint64_t{level} + 2, uint64_t{heights.Grid().Levels});
  if (level >= end) { return result; }
  for (uint32_t at = level; at < end; ++at) {
    if (!heights.Complete(at)) {
      const auto unknown = std::numeric_limits<float>::quiet_NaN();
      return {unknown, unknown};
    }
    const auto [low, high] = std::ranges::minmax_element(heights.Values(at));
    result[0] = std::min(result[0], *low);
    result[1] = std::max(result[1], *high);
  }
  return result;
}

ClipmapFrustum::ClipmapFrustum(const Mat4f &clipFromRelative) noexcept {
  for (size_t side = 0; side < Planes_.size(); ++side) {
    const double sign = side % 2 == 0 ? 1 : -1;
    for (size_t col = 0; col < 4; ++col) {
      const double a = clipFromRelative.At(3, col);
      const double b = clipFromRelative.At(side / 2, col);
      Valid_ &= std::isfinite(a) && std::isfinite(b);
      Planes_[side].Equation[col] = a + sign * b;
      Planes_[side].Error[col] = std::abs(a) + std::abs(b);
    }
  }
}

bool ClipmapFrustum::Intersects(const ClipmapBlock &block,
                                const ClipmapRing &ring,
                                std::array<float, 2> heights,
                                float heightShift) const noexcept {
  if (!Valid_ || !(ring.CentreStep[2] > 0) || heights[0] > heights[1]) { return true; }
  const auto box = Bounds(block, ring, heights, heightShift);
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(box.Low[axis]) || !std::isfinite(box.High[axis]) ||
        box.Low[axis] > box.High[axis] ||
        std::max(std::abs(box.Low[axis]), std::abs(box.High[axis])) >
            std::numeric_limits<float>::max()) {
      return true;
    }
  }
  for (const auto &plane : Planes_) {
    double maximum = plane.Equation[3];
    double uncertainty = plane.Error[3] + 1;
    for (size_t axis = 0; axis < 3; ++axis) {
      maximum +=
          std::max(plane.Equation[axis] * box.Low[axis], plane.Equation[axis] * box.High[axis]);
      uncertainty +=
          plane.Error[axis] * std::max(std::abs(box.Low[axis]), std::abs(box.High[axis]));
    }
    if (maximum < -kGuard * uncertainty) { return false; }
  }
  return true;
}
}
