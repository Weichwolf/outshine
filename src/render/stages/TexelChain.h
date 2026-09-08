#ifndef OUTSHINE_RENDER_STAGES_TEXELCHAIN_H
#define OUTSHINE_RENDER_STAGES_TEXELCHAIN_H

#include <algorithm>
#include "math/Vec3.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine::Render {

struct Texels {
  uint32_t WidthPx = 0;
  uint32_t HeightPx = 0;
};

enum class TexelKind { Value, Direction };

inline void RenormaliseDirection(std::span<float, 4> texel) {
  Vec3f direction;
  float length = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    direction[axis] = texel[static_cast<size_t>(axis)] * 2.0f - 1.0f;
    length += direction[axis] * direction[axis];
  }
  length = std::sqrt(length);
  texel[3] *= length;
  if (length <= 0.0f) { return; }
  for (int axis = 0; axis < 3; ++axis) {
    texel[static_cast<size_t>(axis)] = (direction[axis] / length) * 0.5f + 0.5f;
  }
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
      for (uint32_t sy = static_cast<uint32_t>(top); sy < static_cast<uint32_t>(std::ceil(bottom));
           ++sy) {
        const double height =
            std::min(bottom, static_cast<double>(sy + 1u)) - std::max(top, static_cast<double>(sy));
        for (uint32_t sx = static_cast<uint32_t>(left);
             sx < static_cast<uint32_t>(std::ceil(right));
             ++sx) {
          const double width = std::min(right, static_cast<double>(sx + 1u)) -
                               std::max(left, static_cast<double>(sx));
          const size_t source = (static_cast<size_t>(sy) * fromWidth + sx) * 4u;
          for (size_t channel = 0; channel < 4; ++channel) {
            sum[channel] += from[source + channel] * width * height;
          }
        }
      }
      const size_t at = (static_cast<size_t>(y) * toWidth + x) * 4u;
      for (size_t channel = 0; channel < 4; ++channel) {
        into[at + channel] = static_cast<float>(sum[channel] / area);
      }
      if (kind == TexelKind::Direction) {
        RenormaliseDirection(std::span<float, 4>(into.data() + at, 4));
      }
    }
  }
  return {.WidthPx = toWidth, .HeightPx = toHeight};
}

}

#endif
