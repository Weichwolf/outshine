#include "math/Vec3.h"
#include "Check.h"
#include <cmath>
#include <cstring>
#include <limits>

namespace {
template <typename Number> void CheckRange() {
  using namespace outshine;
  using namespace outshine::Test;
  using Vector = Vector3<Number>;
  const Number maximum = std::numeric_limits<Number>::max();
  const Number smallest = std::numeric_limits<Number>::denorm_min();
  const Number tolerance = Number{8} * std::numeric_limits<Number>::epsilon();
  CHECK(Length(Vector{{maximum, 0, 0}}) == maximum, "representable large axis length stays finite");
  CHECK(Length(Vector{{smallest, 0, 0}}) == smallest, "subnormal axis length is retained");
  Vector triangle{{3, 4, 0}};
  CHECK(Length(triangle) == Number{5} && Normalise(triangle) &&
            std::abs(triangle[0] - Number{3} / Number{5}) <= tolerance &&
            std::abs(triangle[1] - Number{4} / Number{5}) <= tolerance,
        "3-4-5 direction and length match independent analytic values");
  for (const Number scale : {maximum, smallest, Number{1}}) {
    Vector diagonal{{scale, -scale, scale}};
    const bool normalized = Normalise(diagonal);
    const Number component = Number{1} / std::sqrt(Number{3});
    CHECK(normalized && std::abs(diagonal[0] - component) <= tolerance &&
              std::abs(diagonal[1] + component) <= tolerance &&
              std::abs(diagonal[2] - component) <= tolerance,
          "finite nonzero vector normalizes independently of magnitude");
  }
  const Number infinity = std::numeric_limits<Number>::infinity();
  const Number nan = std::numeric_limits<Number>::quiet_NaN();
  for (Vector invalid : {Vector{{0, 0, 0}},
                         Vector{{infinity, 1, 0}},
                         Vector{{1, -infinity, 0}},
                         Vector{{1, 0, nan}}}) {
    const auto before = invalid.Axis;
    CHECK(!Normalise(invalid) && std::memcmp(before.data(), invalid.data(), sizeof(before)) == 0,
          "zero or nonfinite input rejected without changing any component bits");
  }
}
}

int main() {
  CheckRange<float>();
  CheckRange<double>();
  return outshine::Test::Report();
}
