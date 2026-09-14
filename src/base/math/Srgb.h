#ifndef OUTSHINE_BASE_MATH_SRGB_H
#define OUTSHINE_BASE_MATH_SRGB_H

#include <cmath>

namespace outshine::ColourSpace {

inline constexpr float kSrgbEncodedKnee = 0.04045f;
inline constexpr float kSrgbLinearKnee = 0.0031308f;
inline constexpr float kSrgbSlope = 12.92f;
inline constexpr float kSrgbOffset = 0.055f;
inline constexpr float kSrgbScale = 1.055f;
inline constexpr float kSrgbGamma = 2.4f;

[[nodiscard]] inline float LinearFromSrgb(float encoded) noexcept {
  return encoded <= kSrgbEncodedKnee ? encoded / kSrgbSlope
                                     : std::pow((encoded + kSrgbOffset) / kSrgbScale, kSrgbGamma);
}

[[nodiscard]] inline float SrgbFromLinear(float linear) noexcept {
  return linear <= kSrgbLinearKnee ? linear * kSrgbSlope
                                   : kSrgbScale * std::pow(linear, 1.0f / kSrgbGamma) - kSrgbOffset;
}

}
#endif
