#ifndef OUTSHINE_ACTOR_BODY_ROUTESPEEDPROFILE_H
#define OUTSHINE_ACTOR_BODY_ROUTESPEEDPROFILE_H

#include <cstddef>
#include <expected>
#include <functional>
#include <optional>
#include <vector>

#include "math/Vec3.h"

namespace outshine::Motion {

struct RouteSpeedLimits {
  static constexpr double kDefaultMaximumSpeedMps = 30.0;
  static constexpr double kDefaultAccelerationMs2 = 3.0;
  static constexpr double kDefaultBrakingMs2 = 6.0;
  static constexpr double kDefaultLateralAccelerationMs2 = 5.0;
  static constexpr double kDefaultMaximumSampleSpacingM = 5.0;

  double MaximumSpeedMps = kDefaultMaximumSpeedMps;
  double AccelerationMs2 = kDefaultAccelerationMs2;
  double BrakingMs2 = kDefaultBrakingMs2;
  double LateralAccelerationMs2 = kDefaultLateralAccelerationMs2;
  double MaximumSampleSpacingM = kDefaultMaximumSampleSpacingM;
  size_t MaximumSamples = 8192;
};

struct RouteCurveSample {
  Vec3 PositionM;
  Vec3 Forward;
};

struct RouteMotionSample {
  double StationM = 0.0;
  double SpeedMps = 0.0;
};

enum class RouteSpeedError {
  InvalidRoute,
  InvalidLimits,
  SampleBudget,
  MissingSample,
  InvalidSample
};

class RouteSpeedProfile {
public:
  using Sampler = std::function<std::optional<RouteCurveSample>(double)>;

  [[nodiscard]] static std::expected<RouteSpeedProfile, RouteSpeedError>
  Build(double lengthM, const Sampler &sample, RouteSpeedLimits limits = {});

  [[nodiscard]] std::optional<RouteMotionSample> AtTime(double seconds) const noexcept;

  [[nodiscard]] double DurationS() const noexcept { return TimesS_.back(); }

  [[nodiscard]] double LengthM() const noexcept { return StationsM_.back(); }

  [[nodiscard]] size_t SampleCount() const noexcept { return StationsM_.size(); }

private:
  RouteSpeedProfile() = default;

  std::vector<double> StationsM_;
  std::vector<double> TimesS_;
  std::vector<double> SpeedsMps_;
};

}

#endif
