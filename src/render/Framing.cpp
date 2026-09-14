#include "Framing.h"
#include <algorithm>
#include <cmath>
#include <expected>
#include <optional>
#include <string_view>

namespace outshine::Render {
namespace Says {
constexpr auto InvalidFraming = "automatic framing requires finite ordered bounds with positive "
                                "extent, a positive aspect and a finite nonnegative fill";
}

std::expected<Viewpoint, std::string_view> FrameBounds(const Box &bounds,
                                                       FramingOptions options) noexcept {
  const auto &minM = bounds.Min;
  const auto &maxM = bounds.Max;
  const double fill = options.Fill;
  const double aspect = options.Aspect;
  if (!std::isfinite(aspect) || aspect <= 0 || !std::isfinite(fill) || fill < 0) {
    return std::unexpected(Says::InvalidFraming);
  }
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(minM[axis]) || !std::isfinite(maxM[axis]) || minM[axis] > maxM[axis]) {
      return std::unexpected(Says::InvalidFraming);
    }
  }
  const Vec3 span = {{maxM[0] - minM[0], maxM[1] - minM[1], maxM[2] - minM[2]}};
  const double radius = 0.5 * Length(span);
  if (!(radius > 0) || !std::isfinite(radius)) { return std::unexpected(Says::InvalidFraming); }
  Vec3 centre;
  for (int axis = 0; axis < 3; ++axis) { centre[axis] = 0.5 * (minM[axis] + maxM[axis]); }

  const double azimuth = kFramingAzimuthDeg * kDeg2Rad;
  const double elevation = kFramingElevationDeg * kDeg2Rad;

  const Vec3 toEye = {{std::cos(elevation) * std::cos(azimuth),
                       std::sin(elevation),
                       std::cos(elevation) * std::sin(azimuth)}};

  const double yfov = 2.0 * std::atan(kFramingSensorHalfHeightMm / kFramingFocalLengthMm);
  const double halfAngle = std::min(0.5 * yfov, std::atan(std::tan(0.5 * yfov) * aspect));
  const double distance = radius / std::sin(halfAngle) / (fill > 0 ? fill : kFramingFill);
  if (!std::isfinite(distance + radius)) { return std::unexpected(Says::InvalidFraming); }
  Vec3 eye;
  for (int axis = 0; axis < 3; ++axis) { eye[axis] = centre[axis] + toEye[axis] * distance; }

  const std::optional<Viewpoint> seen = Viewpoint::LookAt({.EyeM = eye, .AimM = centre}, 0.0);
  if (!seen) { return std::unexpected(Says::InvalidFraming); }
  Viewpoint out = *seen;
  out.YfovRad = yfov;
  const double floor = radius * kFramingNearFloorFraction;
  out.ZNearM = (distance - radius > floor) ? distance - radius : floor;
  out.ZFarM = distance + radius;
  return out;
}

}
