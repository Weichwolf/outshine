#ifndef OUTSHINE_SCENARIO_BODYVALIDATION_H
#define OUTSHINE_SCENARIO_BODYVALIDATION_H

#include <scenario/Scenario.h>
#include <cmath>
#include <expected>
#include <span>
#include <string_view>

namespace outshine {
namespace Says {
inline constexpr std::string_view InvalidBodyDynamics =
    "body dynamics require finite position, nonnegative finite mass/inertia and a unit quaternion";
}

inline constexpr double kBodyRotationNormTolerance = 1e-6;

[[nodiscard]] inline std::expected<void, std::string_view>
ValidateBodyDynamics(const Scenario::Body &body) noexcept {
  if (!std::isfinite(body.MassKg) || body.MassKg < 0) {
    return std::unexpected(Says::InvalidBodyDynamics);
  }
  for (const double value : body.InertiaKgM2) {
    if (!std::isfinite(value) || value < 0) { return std::unexpected(Says::InvalidBodyDynamics); }
  }
  for (const double value : body.Stands.AtM) {
    if (!std::isfinite(value)) { return std::unexpected(Says::InvalidBodyDynamics); }
  }
  const auto &rotation = body.Stands.Facing;
  const double norm =
      std::hypot(std::hypot(rotation.X, rotation.Y), std::hypot(rotation.Z, rotation.W));
  if (!std::isfinite(norm) || std::abs(norm - 1.0) > kBodyRotationNormTolerance) {
    return std::unexpected(Says::InvalidBodyDynamics);
  }
  return {};
}

[[nodiscard]] inline std::expected<void, std::string_view>
ValidateBodyDynamics(std::span<const Scenario::Body> bodies) noexcept {
  for (const auto &body : bodies) {
    if (const auto valid = ValidateBodyDynamics(body); !valid) { return valid; }
  }
  return {};
}
}
#endif
