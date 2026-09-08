#ifndef OUTSHINE_BASE_SPATIAL_HEIGHTFIELDERROR_H
#define OUTSHINE_BASE_SPATIAL_HEIGHTFIELDERROR_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace outshine::HeightField {

[[nodiscard]] constexpr double
TriangleHeight(double nw, double ne, double sw, double se, double u, double v) {
  return u >= v ? nw + u * (ne - nw) + v * (se - ne) : nw + v * (sw - nw) + u * (se - sw);
}

struct GridRegion {
  size_t X = 0;
  size_t Y = 0;
  size_t Cells = 0;
};

[[nodiscard]] constexpr double
InterpolationErrorM(std::span<const float> reference, size_t side, GridRegion region, size_t grid) {
  if (region.Cells <= grid) { return 0.0; }
  const size_t step = region.Cells / grid;
  const auto at = [&](size_t x, size_t y) {
    return static_cast<double>(reference[(region.Y + y) * side + region.X + x]);
  };
  double error = 0.0;
  for (size_t y = 0; y <= region.Cells; ++y) {
    const size_t y0 = std::min(y / step, grid - 1) * step;
    const double v = static_cast<double>(y - y0) / static_cast<double>(step);
    for (size_t x = 0; x <= region.Cells; ++x) {
      const size_t x0 = std::min(x / step, grid - 1) * step;
      const double u = static_cast<double>(x - x0) / static_cast<double>(step);
      const double height = TriangleHeight(
          at(x0, y0), at(x0 + step, y0), at(x0, y0 + step), at(x0 + step, y0 + step), u, v);
      const double delta = at(x, y) - height;
      error = std::max(error, delta < 0.0 ? -delta : delta);
    }
  }
  return error;
}

[[nodiscard]] constexpr double ProjectedErrorPx(double errorM, double focalPx, double distanceM) {
  return errorM == 0.0 ? 0.0 : errorM * focalPx / distanceM;
}

static_assert(
    [] {
      constexpr size_t side = 5;
      std::array<float, side * side> plane{};
      for (size_t y = 0; y < side; ++y) {
        for (size_t x = 0; x < side; ++x) {
          plane[y * side + x] = static_cast<float>(2 * x + 3 * y);
        }
      }
      constexpr GridRegion whole{.Cells = side - 1};
      if (InterpolationErrorM(plane, side, whole, 1) != 0.0) { return false; }
      constexpr float peakM = 8.0f;
      plane[1 * side + 3] += peakM;
      return InterpolationErrorM(plane, side, whole, 1) == peakM &&
             InterpolationErrorM(plane, side, whole, 2) == peakM &&
             InterpolationErrorM(plane, side, whole, side - 1) == 0.0;
    }(),
    "a steep plane needs no refinement; an off-centre peak missed by the coarse nodes does");
static_assert([] {
  constexpr double errorM = 2.0;
  constexpr double focalPx = 1000.0;
  constexpr double distanceM = 1000.0;
  return ProjectedErrorPx(errorM, focalPx, distanceM) == errorM &&
         ProjectedErrorPx(errorM, 2.0 * focalPx, distanceM) == 2.0 * errorM &&
         ProjectedErrorPx(errorM, focalPx, 2.0 * distanceM) == errorM / 2.0;
}());

}
#endif
