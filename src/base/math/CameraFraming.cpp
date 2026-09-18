#include "CameraFraming.h"

#include <algorithm>
#include <cmath>
#include <expected>
#include <string_view>

#include "math/Units.h"

namespace outshine {
namespace Says {
constexpr auto InvalidCameraFraming =
    "automatic framing requires finite ordered bounds with positive extent, a positive aspect "
    "and a finite nonnegative fill";
}

std::expected<Camera, std::string_view> FrameCamera(const Box &bounds,
                                                    CameraFramingOptions options) noexcept {
  const auto &minM = bounds.Min;
  const auto &maxM = bounds.Max;
  const double fill = options.Fill;
  const double aspect = options.Aspect;
  if (!std::isfinite(aspect) || aspect <= 0 || !std::isfinite(fill) || fill < 0) {
    return std::unexpected(Says::InvalidCameraFraming);
  }
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(minM[axis]) || !std::isfinite(maxM[axis]) || minM[axis] > maxM[axis]) {
      return std::unexpected(Says::InvalidCameraFraming);
    }
  }
  const Vec3 span = {{maxM[0] - minM[0], maxM[1] - minM[1], maxM[2] - minM[2]}};
  const double radius = 0.5 * Length(span);
  if (!(radius > 0) || !std::isfinite(radius)) {
    return std::unexpected(Says::InvalidCameraFraming);
  }
  Vec3 centre;
  for (int axis = 0; axis < 3; ++axis) { centre[axis] = 0.5 * (minM[axis] + maxM[axis]); }

  const double azimuth = kCameraFramingAzimuthDeg * kDeg2Rad;
  const double elevation = kCameraFramingElevationDeg * kDeg2Rad;
  const Vec3 toEye = {{std::cos(elevation) * std::cos(azimuth),
                       std::sin(elevation),
                       std::cos(elevation) * std::sin(azimuth)}};
  const double yfovRad =
      2.0 * std::atan(kCameraFramingSensorHalfHeightMm / kCameraFramingFocalLengthMm);
  const double halfAngle = std::min(0.5 * yfovRad, std::atan(std::tan(0.5 * yfovRad) * aspect));
  const double distance = radius / std::sin(halfAngle) / (fill > 0 ? fill : kCameraFramingFill);
  if (!std::isfinite(distance + radius)) { return std::unexpected(Says::InvalidCameraFraming); }

  Camera camera;
  camera.PositionM = centre + toEye * distance;
  camera.LooksAt = true;
  camera.LookAtM = centre;
  camera.UpM = {{0, 1, 0}};
  const double floor = radius * kCameraFramingNearFloorFraction;
  camera.setProjection(
      Camera::Perspective{.FovDeg = yfovRad * kRad2Deg,
                          .NearM = distance - radius > floor ? distance - radius : floor,
                          .FarM = distance + radius});
  Mat4 validated;
  if (!camera.clipMatrix(aspect, validated)) { return std::unexpected(Says::InvalidCameraFraming); }
  return camera;
}

}
