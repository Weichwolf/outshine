#include "AnimationCurve.h"
#include "Check.h"
#include <array>
#include <limits>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array times{0.0, 1.0};
  const std::array values{0.0, 0.0, 0.0, 2.0, 4.0, 6.0};
  AnimationCurve track;
  CHECK(AnimationCurve::Build(Keyframes::Interpolation::Linear,
                              times,
                              values,
                              3,
                              AnimationCurve::Values::Linear,
                              track),
        "valid translation track built");
  const auto preserved = [&] {
    std::array<double, 3> result{};
    track.At(0.5, result);
    return track.Valid() && track.KeyframeCount() == 2 && track.Components() == 3 &&
           result == std::array{1.0, 2.0, 3.0};
  };
  for (const auto &grid : {std::vector<double>{0, 0},
                           std::vector<double>{1, 0},
                           std::vector<double>{-1, 0},
                           std::vector<double>{0, std::numeric_limits<double>::infinity()},
                           std::vector<double>{0, std::numeric_limits<double>::quiet_NaN()}}) {
    CHECK(!AnimationCurve::Build(Keyframes::Interpolation::Linear,
                                 grid,
                                 values,
                                 3,
                                 AnimationCurve::Values::Linear,
                                 track),
          "invalid time grid rejected");
    CHECK(preserved(), "rejected grid preserves previous track and samples");
    CHECK(AnimationCurve::Build(Keyframes::Interpolation::Linear,
                                times,
                                values,
                                3,
                                AnimationCurve::Values::Linear,
                                track),
          "valid retry after every rejected grid");
  }
  for (const double value :
       {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    auto invalid = values;
    invalid[4] = value;
    CHECK(!AnimationCurve::Build(Keyframes::Interpolation::Linear,
                                 times,
                                 invalid,
                                 3,
                                 AnimationCurve::Values::Linear,
                                 track),
          "nonfinite sample value rejected");
    CHECK(preserved(), "rejected value preserves previous track");
  }
  CHECK(!AnimationCurve::Build(Keyframes::Interpolation::Linear,
                               times,
                               values,
                               0,
                               AnimationCurve::Values::Linear,
                               track),
        "zero-width values are rejected");
  CHECK(!AnimationCurve::Build(Keyframes::Interpolation::Linear,
                               times,
                               values,
                               3,
                               AnimationCurve::Values::Rotation,
                               track),
        "rotation curves require four quaternion components");
  CHECK(!AnimationCurve::Build(Keyframes::Interpolation::Linear,
                               times,
                               values,
                               3,
                               static_cast<AnimationCurve::Values>(255),
                               track),
        "unknown value kinds are rejected");
  CHECK(!AnimationCurve::Build(static_cast<Keyframes::Interpolation>(255),
                               times,
                               values,
                               3,
                               AnimationCurve::Values::Linear,
                               track),
        "unknown interpolation rejected");
  CHECK(preserved(), "invalid enums preserve track");
  CHECK(AnimationCurve::Build(Keyframes::Interpolation::Linear,
                              times,
                              values,
                              3,
                              AnimationCurve::Values::Linear,
                              track),
        "valid track available for sampling");
  std::array<double, 3> output{7, 8, 9};
  track.At(std::numeric_limits<double>::quiet_NaN(), output);
  CHECK((output == std::array{7.0, 8.0, 9.0}), "nonfinite query leaves output untouched");
  return Report();
}
