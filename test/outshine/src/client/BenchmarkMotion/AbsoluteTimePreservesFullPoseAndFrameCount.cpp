#include "BenchmarkMotion.h"
#include "Check.h"
#include "math/Units.h"

#include <array>
#include <cmath>
#include <limits>

namespace {
using namespace outshine;
using namespace outshine::Client;
using namespace outshine::Test;

Vec3 ReferenceRotate(const Vec3 &value, Vec3 omega, double seconds) {
  const double magnitude = std::hypot(omega[0], omega[1], omega[2]);
  if (magnitude == 0) { return value; }
  const Vec3 axis{{omega[0] / magnitude, omega[1] / magnitude, omega[2] / magnitude}};
  const double angle = std::remainder(magnitude * seconds, 360.0) * kDeg2Rad;
  return value * std::cos(angle) + Cross(axis, value) * std::sin(angle) +
         axis * (Dot(axis, value) * (1 - std::cos(angle)));
}

void Compare(const Camera &initial, const BenchmarkMotionInput &input) {
  const auto motion = BenchmarkMotion::Make(initial, input);
  CHECK(motion.has_value(), "a finite motion plan is accepted");
  if (!motion) { return; }
  Mat4 base;
  CHECK(initial.modelMatrix(base).has_value(), "the independent initial basis is valid");
  for (const size_t frame : {input.Frames - 1, size_t{0}, input.Frames / 2}) {
    const auto pose = motion->At(frame);
    CHECK(pose.has_value(), "each requested frame has exactly one derived pose");
    if (!pose) { continue; }
    Mat4 actual;
    CHECK(pose->modelMatrix(actual).has_value(), "the derived quaternion is a valid camera");
    const double seconds = static_cast<double>(frame + 1) / input.RateHz;
    bool equal = true;
    for (size_t column = 0; column < 3; ++column) {
      const Vec3 before{{base[column * 4], base[column * 4 + 1], base[column * 4 + 2]}};
      const auto expected = ReferenceRotate(before, input.AngularVelocityDegS, seconds);
      for (size_t axis = 0; axis < 3; ++axis) {
        equal = equal && std::abs(actual[column * 4 + axis] - expected[axis]) < 1e-12;
      }
    }
    CHECK(equal, "world rotation agrees with independent Rodrigues, including noncommuting starts");
    CHECK(pose->PositionM == initial.PositionM + input.LinearVelocityMps * seconds,
          "motion uses absolute frame time rather than accumulating previous poses");
    CHECK(pose->FovDeg == initial.FovDeg && pose->NearM == initial.NearM &&
              pose->FarM == initial.FarM,
          "a trajectory does not change projection");
  }
  CHECK(!motion->At(input.Frames), "the first frame beyond N is rejected");
}

void InvalidInputs() {
  Camera initial;
  BenchmarkMotionInput input{.Frames = 60, .RateHz = 60};
  input.Frames = 0;
  CHECK(!BenchmarkMotion::Make(initial, input), "zero frames cannot create an empty timing window");
  input.Frames = kMaximumBenchmarkFrames + 1;
  CHECK(!BenchmarkMotion::Make(initial, input), "the timing storage bound is explicit");
  input.Frames = 60;
  input.RateHz = 0;
  CHECK(!BenchmarkMotion::Make(initial, input), "zero rate is rejected before division");
  input.RateHz = std::numeric_limits<double>::quiet_NaN();
  CHECK(!BenchmarkMotion::Make(initial, input), "nonfinite time is rejected");
  input.RateHz = 1;
  input.LinearVelocityMps[0] = std::numeric_limits<double>::max();
  CHECK(!BenchmarkMotion::Make(initial, input), "unrepresentable trajectory extent is rejected");
  input.LinearVelocityMps = {};
  input.AngularVelocityDegS[2] = std::numeric_limits<double>::infinity();
  CHECK(!BenchmarkMotion::Make(initial, input), "nonfinite angular motion is rejected");
}
}

int main() {
  std::array<Camera, 6> starts{};
  starts[1].Orientation = {.X = 1, .Y = 0, .Z = 0, .W = 0};
  starts[2].Orientation = {.X = 0, .Y = 1, .Z = 0, .W = 0};
  starts[3].Orientation = {.X = 0, .Y = 0, .Z = 1, .W = 0};
  starts[4].Orientation = {.X = 0.2, .Y = 0.3, .Z = 0.4, .W = 0.5};
  starts[5].LooksAt = true;
  starts[5].PositionM = {{12, -4, 7}};
  starts[5].LookAtM = {{14, -3, 3}};
  for (const double rate : {25., 30., 60., 120.}) {
    for (auto initial : starts) {
      initial.FovDeg = 71;
      initial.NearM = 0.2;
      initial.FarM = 1300;
      Compare(initial,
              {.Frames = 60,
               .RateHz = rate,
               .LinearVelocityMps = {{2, -1, 3}},
               .AngularVelocityDegS = {{30, 40, 50}}});
    }
  }
  Camera huge;
  huge.PositionM = {{1e20, 1e20, 1e20}};
  const auto large =
      BenchmarkMotion::Make(huge, {.Frames = 1, .RateHz = 1, .AngularVelocityDegS = {{0, 90, 0}}});
  CHECK(large.has_value(), "large positions retain orientation independently of a look-at target");
  if (large) {
    const auto pose = large->At(0);
    Mat4 model;
    CHECK(pose && !pose->LooksAt && pose->modelMatrix(model).has_value() &&
              std::abs(model[8] - 1) < 1e-12 && std::abs(model[10]) < 1e-12,
          "positive world Y turns the original northward view west without position cancellation");
  }
  Compare(Camera{}, {.Frames = 60, .RateHz = 60});
  InvalidInputs();
  return Report();
}
