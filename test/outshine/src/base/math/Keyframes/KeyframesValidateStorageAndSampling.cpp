#include "Keyframes.h"
#include "Check.h"
#include <array>
#include <limits>
#include <span>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array times{-1.0, 1.0};
  const std::array values{1.0, 3.0};
  const auto linear = Keyframes::Build(Keyframes::Interpolation::Linear, times, values, 1);
  const auto step = Keyframes::Build(Keyframes::Interpolation::Step, times, values, 1);
  CHECK(linear && step, "native curves permit negative time coordinates");
  if (!linear || !step) { return Report(); }
  CHECK(linear->AtScalar(0) == 2 && step->AtScalar(0) == 1,
        "linear and held midpoint are analytical");
  CHECK(linear->AtScalar(-2) == 1 && linear->AtScalar(2) == 3,
        "sampling clamps to endpoint values");
  const std::array cubicValues{0.0, 1.0, 2.0, 0.0, 3.0, 0.0};
  const auto cubic = Keyframes::Build(Keyframes::Interpolation::CubicSpline, times, cubicValues, 1);
  CHECK(cubic && cubic->AtScalar(0) == 2.5 && cubic->AtScalar(-1) == 1 && cubic->AtScalar(1) == 3,
        "Hermite interpolation includes the two-second tangent scale");
  for (const size_t width : {size_t{0}, size_t{3}, std::numeric_limits<size_t>::max()}) {
    CHECK(!Keyframes::Build(Keyframes::Interpolation::Linear, times, values, width),
          "invalid widths rejected without multiplied size arithmetic");
  }
  CHECK(!Keyframes::Build(static_cast<Keyframes::Interpolation>(255), times, values, 1),
        "unknown interpolation rejected");
  const std::array reversed{1.0, -1.0};
  CHECK(!Keyframes::Build(Keyframes::Interpolation::Linear, reversed, values, 1),
        "reversed times rejected");
  const std::array unbounded{-std::numeric_limits<double>::max(),
                             std::numeric_limits<double>::max()};
  CHECK(!Keyframes::Build(Keyframes::Interpolation::Linear, unbounded, values, 1),
        "overflowing duration rejected");
  const std::array vectorValues{0.0, 0.0, 0.0, 2.0, 4.0, 6.0};
  const auto vector = Keyframes::Build(Keyframes::Interpolation::Linear, times, vectorValues, 3);
  CHECK(vector.has_value(), "vector curve built");
  if (vector) {
    std::array<double, 4> output{11, 12, 13, 14};
    vector->At(0, std::span(output).first(2));
    CHECK((output == std::array{11.0, 12.0, 13.0, 14.0}),
          "short output and guard remain untouched");
    CHECK(vector->AtScalar(0) == 0, "scalar accessor cannot overwrite its storage for vector data");
  }
  for (const double query :
       {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    std::array<double, 1> output{7};
    linear->At(query, output);
    CHECK(output[0] == 7, "nonfinite query leaves destination unchanged");
    size_t index = 9;
    double weight = 7;
    CHECK(!linear->Span(query, index, weight) && index == 9 && weight == 7,
          "invalid span query preserves outputs");
  }
  return Report();
}
