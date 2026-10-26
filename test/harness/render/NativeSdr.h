#ifndef OUTSHINE_TEST_RENDER_NATIVESDR_H
#define OUTSHINE_TEST_RENDER_NATIVESDR_H

#include "Device.h"
#include <array>
#include <cmath>
#include <utility>

namespace outshine::Test::Gpu {

[[nodiscard]] inline bool IsSrgb(Render::Native::Format format) {
  return format == Render::Native::Format::Bgra8Srgb || format == Render::Native::Format::Rgba8Srgb;
}

[[nodiscard]] inline bool IsBgra(Render::Native::Format format) {
  return format == Render::Native::Format::Bgra8Unorm ||
         format == Render::Native::Format::Bgra8Srgb;
}

[[nodiscard]] inline bool IsSdr(Render::Native::Format format) {
  return IsBgra(format) || format == Render::Native::Format::Rgba8Unorm ||
         format == Render::Native::Format::Rgba8Srgb;
}

[[nodiscard]] inline int SdrByte(double linear, Render::Native::Format format) {
  constexpr double kLinearThreshold = 0.0031308;
  constexpr double kLinearSlope = 12.92;
  constexpr double kPowerGain = 1.055;
  constexpr double kPowerOffset = 0.055;
  constexpr double kGamma = 2.4;
  constexpr double kByteMaximum = 255.0;
  if (IsSrgb(format)) {
    linear = linear <= kLinearThreshold
                 ? kLinearSlope * linear
                 : kPowerGain * std::pow(linear, 1.0 / kGamma) - kPowerOffset;
  }
  return static_cast<int>(std::lround(kByteMaximum * linear));
}

[[nodiscard]] inline std::array<int, 4>
SdrPixel(double red, double green, double blue, Render::Native::Format format) {
  constexpr int kOpaqueAlpha = 255;
  std::array result{
      SdrByte(red, format), SdrByte(green, format), SdrByte(blue, format), kOpaqueAlpha};
  if (IsBgra(format)) { std::swap(result[0], result[2]); }
  return result;
}

}
#endif
