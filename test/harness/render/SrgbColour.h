#ifndef OUTSHINE_TEST_RENDER_SRGBCOLOUR_H
#define OUTSHINE_TEST_RENDER_SRGBCOLOUR_H

#include <cmath>
#include <cstdint>
#include <optional>

namespace outshine::Test {

[[nodiscard]] inline std::optional<uint8_t> EncodeSrgb8(double linear) {
  if (!std::isfinite(linear) || linear < 0 || linear > 1) { return std::nullopt; }
  const double encoded =
      linear <= 0.0031308 ? linear * 12.92 : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
  return static_cast<uint8_t>(std::lround(encoded * 255));
}

}
#endif
