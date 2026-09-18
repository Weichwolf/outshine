#ifndef OUTSHINE_BASE_MATH_CAMERAFRAMING_H
#define OUTSHINE_BASE_MATH_CAMERAFRAMING_H

#include <expected>
#include <string_view>

#include "math/Box.h"
#include "scene/Camera.h"

namespace outshine {

constexpr double kCameraFramingAzimuthDeg = 35.0;
constexpr double kCameraFramingElevationDeg = 20.0;
constexpr double kCameraFramingSensorHalfHeightMm = 12.0;
constexpr double kCameraFramingFocalLengthMm = 50.0;
constexpr double kCameraFramingFill = 0.95;
constexpr double kCameraFramingNearFloorFraction = 0.001;

struct CameraFramingOptions {
  double Fill = kCameraFramingFill;
  double Aspect = 1.0;
};

[[nodiscard]] std::expected<Camera, std::string_view>
FrameCamera(const Box &bounds, CameraFramingOptions options = {}) noexcept;

}

#endif
