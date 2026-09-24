#include "RouteSpeedProfile.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

namespace outshine::Motion {
namespace {

[[nodiscard]] bool Valid(RouteSpeedLimits limits) noexcept {
  return std::isfinite(limits.MaximumSpeedMps) && limits.MaximumSpeedMps > 0.0 &&
         std::isfinite(limits.AccelerationMs2) && limits.AccelerationMs2 > 0.0 &&
         std::isfinite(limits.BrakingMs2) && limits.BrakingMs2 > 0.0 &&
         std::isfinite(limits.LateralAccelerationMs2) && limits.LateralAccelerationMs2 > 0.0 &&
         std::isfinite(limits.MaximumSampleSpacingM) && limits.MaximumSampleSpacingM > 0.0 &&
         limits.MaximumSamples >= 3;
}

[[nodiscard]] bool Finite(const Vec3 &value) noexcept {
  return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

[[nodiscard]] bool LimitCurveSpeed(std::span<double> speeds,
                                   std::span<const Vec3> forwards,
                                   double lengthM,
                                   RouteSpeedLimits limits) noexcept {
  const double spacingM = lengthM / static_cast<double>(forwards.size() - 1);
  for (size_t index = 1; index < forwards.size(); ++index) {
    const double cosine = std::clamp(Dot(forwards[index - 1], forwards[index]), -1.0, 1.0);
    const double curvature = std::acos(cosine) / spacingM;
    if (!std::isfinite(curvature)) { return false; }
    if (curvature > 0.0) {
      const double allowed = std::sqrt(limits.LateralAccelerationMs2 / curvature);
      speeds[index - 1] = std::min(speeds[index - 1], allowed);
      speeds[index] = std::min(speeds[index], allowed);
    }
  }
  return true;
}

}

std::expected<RouteSpeedProfile, RouteSpeedError>
RouteSpeedProfile::Build(double lengthM, const Sampler &sample, RouteSpeedLimits limits) {
  if (!std::isfinite(lengthM) || lengthM <= 0.0 || !sample) {
    return std::unexpected(RouteSpeedError::InvalidRoute);
  }
  if (!Valid(limits)) { return std::unexpected(RouteSpeedError::InvalidLimits); }
  const double intervalCount = std::max(2.0, std::ceil(lengthM / limits.MaximumSampleSpacingM));
  if (!std::isfinite(intervalCount) ||
      intervalCount + 1.0 > static_cast<double>(limits.MaximumSamples)) {
    return std::unexpected(RouteSpeedError::SampleBudget);
  }
  const auto intervals = static_cast<size_t>(intervalCount);
  const double spacingM = lengthM / static_cast<double>(intervals);
  std::vector<Vec3> forwards;
  forwards.reserve(intervals + 1);
  RouteSpeedProfile profile;
  profile.StationsM_.reserve(intervals + 1);
  profile.TimesS_.reserve(intervals + 1);
  profile.SpeedsMps_.assign(intervals + 1, limits.MaximumSpeedMps);
  for (size_t index = 0; index <= intervals; ++index) {
    const double stationM = lengthM * static_cast<double>(index) / static_cast<double>(intervals);
    const auto pose = sample(stationM);
    if (!pose) { return std::unexpected(RouteSpeedError::MissingSample); }
    if (!Finite(pose->PositionM) || !Finite(pose->Forward)) {
      return std::unexpected(RouteSpeedError::InvalidSample);
    }
    Vec3 forward = pose->Forward;
    if (!Normalise(forward)) { return std::unexpected(RouteSpeedError::InvalidSample); }
    profile.StationsM_.push_back(stationM);
    forwards.push_back(forward);
  }
  if (!LimitCurveSpeed(profile.SpeedsMps_, forwards, lengthM, limits)) {
    return std::unexpected(RouteSpeedError::InvalidSample);
  }
  profile.SpeedsMps_.front() = 0.0;
  for (size_t index = 1; index <= intervals; ++index) {
    const double prior = profile.SpeedsMps_[index - 1];
    profile.SpeedsMps_[index] =
        std::min(profile.SpeedsMps_[index],
                 std::sqrt(prior * prior + 2.0 * limits.AccelerationMs2 * spacingM));
  }
  profile.SpeedsMps_.back() = 0.0;
  for (size_t index = intervals; index > 0; --index) {
    const double next = profile.SpeedsMps_[index];
    profile.SpeedsMps_[index - 1] = std::min(
        profile.SpeedsMps_[index - 1], std::sqrt(next * next + 2.0 * limits.BrakingMs2 * spacingM));
  }
  profile.TimesS_.push_back(0.0);
  for (size_t index = 1; index <= intervals; ++index) {
    const double speedSum = profile.SpeedsMps_[index - 1] + profile.SpeedsMps_[index];
    if (!std::isfinite(speedSum) || speedSum <= 0.0) {
      return std::unexpected(RouteSpeedError::InvalidSample);
    }
    const double nextS = profile.TimesS_.back() + 2.0 * spacingM / speedSum;
    if (!std::isfinite(nextS) || nextS <= profile.TimesS_.back()) {
      return std::unexpected(RouteSpeedError::InvalidSample);
    }
    profile.TimesS_.push_back(nextS);
  }
  return profile;
}

std::optional<RouteMotionSample> RouteSpeedProfile::AtTime(double seconds) const noexcept {
  if (!std::isfinite(seconds) || seconds < 0.0 || TimesS_.empty()) { return std::nullopt; }
  if (seconds >= DurationS()) { return RouteMotionSample{.StationM = LengthM(), .SpeedMps = 0.0}; }
  const auto upper = std::ranges::upper_bound(TimesS_, seconds);
  const size_t index = static_cast<size_t>(upper - TimesS_.begin() - 1);
  const double elapsedS = seconds - TimesS_[index];
  const double intervalS = TimesS_[index + 1] - TimesS_[index];
  const double accelerationMs2 = (SpeedsMps_[index + 1] - SpeedsMps_[index]) / intervalS;
  const double stationM = StationsM_[index] + SpeedsMps_[index] * elapsedS +
                          0.5 * accelerationMs2 * elapsedS * elapsedS;
  return RouteMotionSample{.StationM =
                               std::clamp(stationM, StationsM_[index], StationsM_[index + 1]),
                           .SpeedMps = SpeedsMps_[index] + accelerationMs2 * elapsedS};
}

}
