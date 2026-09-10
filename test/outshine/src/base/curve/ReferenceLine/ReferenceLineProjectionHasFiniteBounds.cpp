#include <cmath>
#include <limits>
#include "src/base/curve/ReferenceLine.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  ReferenceLine line;
  std::string error;
  CHECK(line.Lay({}, {{.LengthM = 100}}, error), "straight reference");
  for (double east : {-10.0, 0.0, 37.0, 100.0, 110.0}) {
    const auto found = line.Nearest({east, 4}, {50, 50});
    CHECK(found.has_value(), "finite straight projection exists");
    if (found) {
      const double expected = east < 0 ? 0 : (east > 100 ? 100 : east);
      CHECK_NEAR(*found, expected, 1e-4, "m", "analytic clamped projection");
    }
  }
  CHECK(line.Nearest({-1, 0}, {50, 50}).value_or(-1) == 0,
        "exact endpoint minimum survives refinement");
  CHECK(line.Nearest({101, 0}, {50, 50}).value_or(-1) == 100,
        "exact far endpoint minimum survives refinement");
  for (double invalid : {std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()}) {
    CHECK(!line.Nearest({invalid, 0}, {50, 50}), "invalid east coordinate");
    CHECK(!line.Nearest({0, invalid}, {50, 50}), "invalid north coordinate");
    CHECK(!line.Nearest({0, 0}, {invalid, 50}), "invalid window centre");
    CHECK(!line.Nearest({0, 0}, {50, invalid}), "invalid window radius");
  }
  const double maximum = std::numeric_limits<double>::max();
  CHECK(!line.Nearest({0, 0}, {maximum, maximum}), "overflowing window rejected");
  CHECK(!line.Nearest({0, 0}, {50, 0}), "empty window rejected");
  CHECK(line.Lay({}, {{.LengthM = 1e160}}, error), "large representable straight");
  const auto large = line.Nearest({7.5e159, 1e158}, {5e159, 5e159});
  CHECK(large && std::isfinite(*large), "large projection has finite distance");
  if (large) {
    CHECK_NEAR(*large / 1e160, 0.75, 1e-6, "fraction", "distance squares must not overflow");
  }
  return Report();
}
