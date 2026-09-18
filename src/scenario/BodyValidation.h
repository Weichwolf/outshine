#ifndef OUTSHINE_SCENARIO_BODYVALIDATION_H
#define OUTSHINE_SCENARIO_BODYVALIDATION_H

#include <scenario/Scenario.h>
#include <cmath>
#include <array>
#include <expected>
#include <span>
#include <string_view>

namespace outshine {
namespace Says {
inline constexpr std::string_view InvalidBodyDrive =
    "body drive requires a known category, finite parameters, nonnegative peaks and circle, a "
    "nonzero axis and a matching force or torque channel";
inline constexpr std::string_view InvalidBodyDynamics =
    "body dynamics require finite position, nonnegative finite mass/inertia and a unit quaternion";
}

inline constexpr double kBodyRotationNormTolerance = 1e-6;

[[nodiscard]] inline std::expected<void, std::string_view>
ValidateBodyDrive(const Scenario::Drive &drive) noexcept {
  if (drive.Does != Scenario::Drives::Effort && drive.Does != Scenario::Drives::Motion) {
    return std::unexpected(Says::InvalidBodyDrive);
  }
  for (const double value : std::array{drive.PeakNm, drive.PeakN, drive.CircleM}) {
    if (!std::isfinite(value) || value < 0) { return std::unexpected(Says::InvalidBodyDrive); }
  }
  if (!std::isfinite(drive.Ratio) || (drive.Turns ? drive.PeakN != 0 : drive.PeakNm != 0)) {
    return std::unexpected(Says::InvalidBodyDrive);
  }
  bool nonzero = false;
  for (const double value : drive.AxisXyz) {
    if (!std::isfinite(value)) { return std::unexpected(Says::InvalidBodyDrive); }
    nonzero = nonzero || value != 0;
  }
  if (!nonzero) { return std::unexpected(Says::InvalidBodyDrive); }
  return {};
}

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
  for (const auto &drive : body.Driven) {
    if (const auto valid = ValidateBodyDrive(drive); !valid) { return valid; }
  }
  for (const auto &contact : body.Contacts) {
    if (const auto valid = Physics::ValidatePrismaticJoint(contact.Strut); !valid) { return valid; }
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
