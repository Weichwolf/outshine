#ifndef OUTSHINE_ENGINE_GEODETICCAMERA_H
#define OUTSHINE_ENGINE_GEODETICCAMERA_H

#include "GroundQuery.h"
#include "math/Units.h"
#include "math/RenderFrame.h"
#include "TangentFrame.h"
#include <scenario/Scenario.h>
#include <cmath>
#include <expected>
#include <optional>
#include <string_view>

namespace outshine {
namespace Says {
inline constexpr std::string_view kCameraGroundRequired =
    "geodetic camera requires declared ground for height sampling";
inline constexpr std::string_view kCameraGroundHole =
    "geodetic camera height is unavailable in the terrain source";
inline constexpr std::string_view kCameraPositionInvalid =
    "geodetic camera coordinates or offset are invalid";
}

[[nodiscard]] inline std::expected<std::optional<Vec3>, std::string_view> ResolveGeodeticCamera(
    const Scenario::View &camera, LongitudeLatitude origin, const GroundQuery *ground) {
  constexpr double kPoleDeg = kDegPerHalfTurn / 2.0;
  auto point = camera.Geographic.Geodetic;
  if (!std::isfinite(point.LongitudeDeg) || std::abs(point.LongitudeDeg) > kDegPerHalfTurn ||
      !std::isfinite(point.LatitudeDeg) || std::abs(point.LatitudeDeg) > kPoleDeg ||
      !std::isfinite(point.HeightM) || !std::isfinite(origin.LongitudeDeg) ||
      std::abs(origin.LongitudeDeg) > kDegPerHalfTurn || !std::isfinite(origin.LatitudeDeg) ||
      std::abs(origin.LatitudeDeg) > kPoleDeg) {
    return std::unexpected(Says::kCameraPositionInvalid);
  }
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(camera.OffsetM[axis])) {
      return std::unexpected(Says::kCameraPositionInvalid);
    }
  }
  if (camera.Geographic.SamplesHeight) {
    if (ground == nullptr) { return std::unexpected(Says::kCameraGroundRequired); }
    const auto sample =
        ground->At({.LongitudeDeg = point.LongitudeDeg, .LatitudeDeg = point.LatitudeDeg});
    if (sample.Where() == GroundSample::State::Pending) { return std::optional<Vec3>{}; }
    const auto height = sample.AslM();
    if (!height) { return std::unexpected(Says::kCameraGroundHole); }
    point.HeightM += *height;
  }
  const auto position = TangentFrame::At(origin).ToLocalPosition(point);
  const Vec3 station =
      Vec3{{position.EastM, position.UpM, RenderFrame::ZOfNorth(position.NorthM)}} + camera.OffsetM;
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(station[axis])) { return std::unexpected(Says::kCameraPositionInvalid); }
  }
  return station;
}

struct GeographicCameraAxes {
  Vec3 Forward;
  Vec3 Up;
};

[[nodiscard]] inline std::expected<GeographicCameraAxes, std::string_view>
ResolveGeodeticCameraAxes(const Scenario::GeographicCameraPlacement &camera,
                          LongitudeLatitude origin) noexcept {
  if (!std::isfinite(camera.BearingDeg) || !std::isfinite(camera.PitchDeg)) {
    return std::unexpected(Says::kCameraPositionInvalid);
  }
  const double bearing = std::remainder(camera.BearingDeg, kDegPerHalfTurn * 2.0) * kDeg2Rad;
  const double pitch = std::remainder(camera.PitchDeg, kDegPerHalfTurn * 2.0) * kDeg2Rad;
  const EnuAxes axes = EnuAxesEcef(camera.Geodetic);
  const Vec3 horizontal = axes.East * std::sin(bearing) + axes.North * std::cos(bearing);
  const TangentFrame frame = TangentFrame::At(origin);
  const auto native = [&](const Vec3 &direction) {
    const auto local = frame.ToLocalDirection(direction);
    return Vec3{{local.EastM, local.UpM, RenderFrame::ZOfNorth(local.NorthM)}};
  };
  const GeographicCameraAxes result{
      .Forward = native(horizontal * std::cos(pitch) + axes.Up * std::sin(pitch)),
      .Up = native(axes.Up * std::cos(pitch) - horizontal * std::sin(pitch))};
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(result.Forward[axis]) || !std::isfinite(result.Up[axis])) {
      return std::unexpected(Says::kCameraPositionInvalid);
    }
  }
  return result;
}

}
#endif
