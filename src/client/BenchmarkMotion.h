#ifndef OUTSHINE_CLIENT_BENCHMARKMOTION_H
#define OUTSHINE_CLIENT_BENCHMARKMOTION_H

#include <Camera.h>
#include <cstddef>
#include <expected>
#include <string_view>

namespace outshine::Client {

inline constexpr size_t kMaximumBenchmarkFrames = 1'000'000;

struct BenchmarkMotionInput {
  size_t Frames = 0;
  double RateHz = 0;
  Vec3 LinearVelocityMps{};
  Vec3 AngularVelocityDegS{};
};

class BenchmarkMotion {
public:
  [[nodiscard]] static std::expected<BenchmarkMotion, std::string_view>
  Make(const Camera &initial, BenchmarkMotionInput input) noexcept;

  [[nodiscard]] std::expected<Camera, std::string_view> At(size_t frame) const noexcept;

private:
  Camera Initial_{};
  BenchmarkMotionInput Input_{};
  Quat Rotation_;
  Vec3 Axis_{};
  double SpeedDegS_ = 0;
};

}

#endif
