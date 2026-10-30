#include "Check.h"
#include "RoadGroundAffine.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stop_token>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr double kLengthM = 100;
constexpr double kHeightM = 12;
constexpr double kLongGrade = .05;
constexpr double kCrossGrade = .025;
constexpr double kAlong = .03;
constexpr double kAcross = .005;

[[nodiscard]] RoadGroundField Input(RoadPlane gradient) {
  RoadGroundField input;
  input.Patches.resize(1);
  for (const auto at : std::array<EastNorth, 4>{
           {{}, {.EastM = kLengthM}, {.EastM = kLengthM, .NorthM = 2}, {.NorthM = 2}}}) {
    input.Priors.push_back(
        {.At = at, .HeightM = kHeightM + at.EastM * gradient.SlopeE + at.NorthM * gradient.SlopeN});
  }
  input.Faces.push_back({.Triangle = {input.Priors[0].At, input.Priors[1].At, input.Priors[2].At},
                         .GradeFrame = {1 / kLongGrade, 0, 0, 1 / kCrossGrade}});
  input.Faces.push_back({.Triangle = {input.Priors[0].At, input.Priors[2].At, input.Priors[3].At},
                         .GradeFrame = {1 / kLongGrade, 0, 0, 1 / kCrossGrade}});
  return input;
}
}

int main() {
  const auto input = Input({.SlopeE = kAlong, .SlopeN = kAcross});
  const auto fit = FitRoadGroundAffine(input);
  CHECK(fit && fit->MaximumPriorErrorM < 1e-12 && fit->MaximumNormalizedGrade < 1,
        "an exact full-width affine prior is recovered without a solver");
  if (fit) {
    const auto sample = fit->Field.Evaluate(0, {.EastM = kLengthM / 2, .NorthM = 1});
    CHECK(sample && std::abs(sample->Gradient.SlopeE - kAlong) < 1e-12 &&
              std::abs(sample->Gradient.SlopeN - kAcross) < 1e-12 &&
              fit->Observations == input.Priors.size() && fit->Rotations <= 3 * input.Priors.size(),
          "the three-column QR retains the original gradient and bounded work");
  }
  const auto steep = FitRoadGroundAffine(Input({.SlopeE = .2, .SlopeN = .1}));
  CHECK(steep && steep->MaximumNormalizedGrade <= 1 && steep->MaximumPriorErrorM > .5,
        "common gradient scaling respects the ellipse and reports the original prior loss");
  if (steep) {
    const auto sample = steep->Field.Evaluate(0, {});
    CHECK(sample && std::abs(sample->Gradient.SlopeE / sample->Gradient.SlopeN - 2) < 1e-12,
          "clamping preserves the joint gradient direction instead of clipping components "
          "separately");
  }
  auto missing = input;
  missing.Priors[0].HeightM = std::numeric_limits<double>::quiet_NaN();
  CHECK(!FitRoadGroundAffine(missing),
        "missing original observations cannot become an affine candidate");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = FitRoadGroundAffine(input, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == RoadGroundError::Canceled,
        "cancellation is checked before the affine control");
  const auto capped = FitRoadGroundAffine(input, {.ContainerBytes = 0});
  CHECK(!capped && capped.error() == RoadGroundError::CapacityExceeded,
        "affine storage admission precedes preparation");
  return Report();
}
