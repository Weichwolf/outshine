#include "BenchmarkMotion.h"
#include "FrameTiming.h"
#include "Rotation.h"
#include "Units.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <string_view>

namespace outshine::App {
namespace {

Quat Product(const Quat &left, const Quat &right) noexcept {
  return {.X = left.W * right.X + left.X * right.W + left.Y * right.Z - left.Z * right.Y,
          .Y = left.W * right.Y - left.X * right.Z + left.Y * right.W + left.Z * right.X,
          .Z = left.W * right.Z + left.X * right.Y - left.Y * right.X + left.Z * right.W,
          .W = left.W * right.W - left.X * right.X - left.Y * right.Y - left.Z * right.Z};
}

}

std::expected<BenchmarkMotion, std::string_view>
BenchmarkMotion::Make(const Camera &initial, BenchmarkMotionInput input) noexcept {
  if (input.Frames == 0 || input.Frames > kMaximumBenchmarkFrames || !std::isfinite(input.RateHz) ||
      input.RateHz <= 0 || input.RateHz > kNanosecondsPerSecond) {
    return std::unexpected("benchmark requires a positive rate and 1..1000000 frames");
  }
  const double duration = static_cast<double>(input.Frames) / input.RateHz;
  const double nanoseconds = duration * 1e9;
  if (!std::isfinite(nanoseconds) ||
      nanoseconds >= static_cast<double>(std::numeric_limits<uint64_t>::max())) {
    return std::unexpected("benchmark duration exceeds the native clock range");
  }
  Mat4 matrix;
  if (!initial.viewMatrix(matrix)) { return std::unexpected("benchmark start pose is invalid"); }
  if (!initial.modelMatrix(matrix)) { return std::unexpected("benchmark start pose is invalid"); }
  BenchmarkMotion motion;
  motion.Initial_ = initial;
  motion.Input_ = input;
  motion.Rotation_ = UnitRotationOf(matrix);
  for (size_t axis = 0; axis < 3; ++axis) {
    const double end = initial.PositionM[axis] + input.LinearVelocityMps[axis] * duration;
    if (!std::isfinite(end)) { return std::unexpected("benchmark movement is not representable"); }
  }
  motion.SpeedDegS_ = std::hypot(
      input.AngularVelocityDegS[0], input.AngularVelocityDegS[1], input.AngularVelocityDegS[2]);
  if (!std::isfinite(motion.SpeedDegS_ * duration)) {
    return std::unexpected("benchmark rotation is not representable");
  }
  if (motion.SpeedDegS_ > 0) {
    for (size_t axis = 0; axis < 3; ++axis) {
      motion.Axis_[axis] = input.AngularVelocityDegS[axis] / motion.SpeedDegS_;
    }
  }
  return motion;
}

std::expected<Camera, std::string_view> BenchmarkMotion::At(size_t frame) const noexcept {
  if (frame >= Input_.Frames) { return std::unexpected("benchmark frame is outside its window"); }
  const double seconds = static_cast<double>(frame + 1) / Input_.RateHz;
  const double angle = std::remainder(SpeedDegS_ * seconds, 2 * kDegPerHalfTurn) * kDeg2Rad / 2;
  const double sine = std::sin(angle);
  const Quat delta{
      .X = Axis_[0] * sine, .Y = Axis_[1] * sine, .Z = Axis_[2] * sine, .W = std::cos(angle)};
  Camera pose = Initial_;
  pose.LooksAt = false;
  pose.PositionM = Initial_.PositionM + Input_.LinearVelocityMps * seconds;
  pose.Orientation = Product(delta, Rotation_);
  return pose;
}

}
