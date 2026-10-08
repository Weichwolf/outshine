#ifndef OUTSHINE_CONTENT_SHADE_TEXELCHAIN_H
#define OUTSHINE_CONTENT_SHADE_TEXELCHAIN_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine::Core {

struct Texels {
  uint32_t WidthPx = 0;
  uint32_t HeightPx = 0;
};

enum class TexelKind { Value, Direction };

inline void AddTexelMoment(std::span<const float, 4> source,
                           double area,
                           TexelKind kind,
                           std::array<double, 4> &sum) {
  if (kind == TexelKind::Value) {
    for (size_t channel = 0; channel < 4; ++channel) { sum[channel] += source[channel] * area; }
    return;
  }
  std::array<double, 3> normal{};
  double lengthSquared = 0.0;
  for (size_t channel = 0; channel < 3; ++channel) {
    normal[channel] = static_cast<double>(source[channel]) * 2.0 - 1.0;
    lengthSquared += normal[channel] * normal[channel];
  }
  if (lengthSquared <= 0.0) { return; }
  const double weight =
      std::clamp(static_cast<double>(source[3]), 0.0, 1.0) * area / std::sqrt(lengthSquared);
  for (size_t channel = 0; channel < 3; ++channel) { sum[channel] += normal[channel] * weight; }
}

inline void WriteFilteredTexel(const std::array<double, 4> &sum,
                               TexelKind kind,
                               double area,
                               std::span<float, 4> output) {
  if (kind == TexelKind::Value) {
    for (size_t channel = 0; channel < 4; ++channel) {
      output[channel] = static_cast<float>(sum[channel] / area);
    }
    return;
  }
  const double length = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
  output[0] = length > 0.0 ? static_cast<float>(sum[0] / length * 0.5 + 0.5) : 0.5f;
  output[1] = length > 0.0 ? static_cast<float>(sum[1] / length * 0.5 + 0.5) : 0.5f;
  output[2] = length > 0.0 ? static_cast<float>(sum[2] / length * 0.5 + 0.5) : 1.0f;
  output[3] = static_cast<float>(std::clamp(length / area, 0.0, 1.0));
}

inline Texels
HalveInPlace(std::span<const float> from, Texels was, std::vector<float> &into, TexelKind kind) {
  const uint32_t fromWidth = was.WidthPx;
  const uint32_t fromHeight = was.HeightPx;
  const uint32_t toWidth = fromWidth > 1 ? fromWidth / 2u : 1u;
  const uint32_t toHeight = fromHeight > 1 ? fromHeight / 2u : 1u;
  into.assign(static_cast<size_t>(toWidth) * toHeight * 4u, 0.0f);
  for (uint32_t y = 0; y < toHeight; ++y) {
    for (uint32_t x = 0; x < toWidth; ++x) {
      const double left = static_cast<double>(x) * fromWidth / toWidth;
      const double right = static_cast<double>(x + 1u) * fromWidth / toWidth;
      const double top = static_cast<double>(y) * fromHeight / toHeight;
      const double bottom = static_cast<double>(y + 1u) * fromHeight / toHeight;
      const double area = (right - left) * (bottom - top);
      std::array<double, 4> sum{};
      for (auto sy = static_cast<uint32_t>(top); sy < static_cast<uint32_t>(std::ceil(bottom));
           ++sy) {
        const double height =
            std::min(bottom, static_cast<double>(sy + 1u)) - std::max(top, static_cast<double>(sy));
        for (auto sx = static_cast<uint32_t>(left); sx < static_cast<uint32_t>(std::ceil(right));
             ++sx) {
          const double width = std::min(right, static_cast<double>(sx + 1u)) -
                               std::max(left, static_cast<double>(sx));
          const size_t source = (static_cast<size_t>(sy) * fromWidth + sx) * 4u;
          AddTexelMoment(
              std::span<const float, 4>(from.data() + source, 4), width * height, kind, sum);
        }
      }
      const size_t at = (static_cast<size_t>(y) * toWidth + x) * 4u;
      WriteFilteredTexel(sum, kind, area, std::span<float, 4>(into.data() + at, 4));
    }
  }
  return {.WidthPx = toWidth, .HeightPx = toHeight};
}

}

#endif
