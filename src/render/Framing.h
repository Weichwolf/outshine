#ifndef OUTSHINE_RENDER_FRAMING_H
#define OUTSHINE_RENDER_FRAMING_H

#include "math/Box.h"
#include "Viewing.h"
#include <expected>
#include <string_view>

namespace outshine::Render {

constexpr double kFramingAzimuthDeg = 35.0;
constexpr double kFramingElevationDeg = 20.0;

constexpr double kFramingSensorHalfHeightMm = 12.0;
constexpr double kFramingFocalLengthMm = 50.0;

constexpr double kFramingFill = 0.95;

constexpr double kFramingNearFloorFraction = 0.001;

struct FramingOptions {
  double Fill = kFramingFill;
  double Aspect = 1.0;
};

[[nodiscard]] std::expected<Viewpoint, std::string_view>
FrameBounds(const Box &bounds, FramingOptions options = {}) noexcept;

}
#endif
