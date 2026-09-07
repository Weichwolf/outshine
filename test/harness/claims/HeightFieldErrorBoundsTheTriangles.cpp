#include <array>
#include <limits>

#include "Check.h"
#include "HeightFieldError.h"

int main() {
  using namespace outshine::HeightField;
  using namespace outshine::Test;
  constexpr size_t side = 9;
  constexpr GridRegion whole{.Cells = side - 1};
  std::array<float, side * side> heights{};
  for (size_t y = 0; y < side; ++y) {
    for (size_t x = 0; x < side; ++x) { heights[y * side + x] = static_cast<float>(x * x + y * y); }
  }
  // z=x²+y²: the secant error is x(8-x)+y(8-y), maximised at (4,4).
  CHECK(InterpolationErrorM(heights, side, whole, 1) == 32.0,
        "the measured triangle error equals the paraboloid's closed-form maximum");
  CHECK(InterpolationErrorM(heights, side, whole, 2) == 8.0,
        "halving the spacing quarters a quadratic surface's interpolation error");
  CHECK(InterpolationErrorM(heights, side, whole, side - 1) == 0.0,
        "the reference resolution introduces no further geometric error");

  for (size_t y = 0; y < side; ++y) {
    for (size_t x = 0; x < side; ++x) { heights[y * side + x] = static_cast<float>(x * y); }
  }
  // Bilinear interpolation would incorrectly report zero for this saddle. The rendered
  // triangles use the NW-SE diagonal and have a maximum error of 16 at the centre.
  CHECK(InterpolationErrorM(heights, side, whole, 1) == 16.0,
        "the oracle measures the triangle surface, not the bilinear height sampler");
  const double projected = ProjectedErrorPx(16.0, 1000.0, 8000.0);
  CHECK(projected == 2.0 && projected > 1.0,
        "a sixteen-metre error at eight kilometres exceeds one pixel at this focal length");
  CHECK(!(projected > std::numeric_limits<double>::infinity()),
        "negative control: infinite tolerance accepts the same demonstrably wrong surface");
  CHECK(ProjectedErrorPx(16.0, 2000.0, 8000.0) == 2.0 * projected,
        "doubling focal length doubles projected error at unchanged distance");
  Covers(
      "board:2166 -- terrain refinement measures its rendered triangles against a finer surface");
  return Report();
}
