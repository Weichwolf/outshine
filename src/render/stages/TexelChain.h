#ifndef OUTSHINE_RENDER_STAGES_TEXELCHAIN_H
#define OUTSHINE_RENDER_STAGES_TEXELCHAIN_H

#include "math/Vec2.h"
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

inline uint32_t IndexChannelsOf(std::span<const float> texels) {
  uint32_t mask = 0;
  for (uint32_t channel = 0; channel < 4; ++channel) {
    Vec2f seen = {{0.0f, 0.0f}};
    uint32_t distinct = 0;
    bool third = false;
    for (size_t at = channel; at < texels.size(); at += 4) {
      const float value = texels[at];
      if ((distinct > 0 && value == seen[0]) || (distinct > 1 && value == seen[1])) { continue; }
      if (distinct >= 2) {
        third = true;
        break;
      }
      seen[distinct++] = value;
    }
    if (!third) { mask |= 1u << channel; }
  }
  return mask;
}

constexpr float kOverFourSamples = 0.25f;

struct TexelAt {
  uint32_t X = 0;
  uint32_t Y = 0;
};

inline std::array<size_t, 4> FourUnder(TexelAt to, Texels was) {
  const uint32_t x0 = was.WidthPx > 1 ? to.X * 2u : 0u;
  const uint32_t x1 = was.WidthPx > 1 ? to.X * 2u + 1u : 0u;
  const uint32_t y0 = was.HeightPx > 1 ? to.Y * 2u : 0u;
  const uint32_t y1 = was.HeightPx > 1 ? to.Y * 2u + 1u : 0u;
  return {{(static_cast<size_t>(y0) * was.WidthPx + x0) * 4u,
           (static_cast<size_t>(y0) * was.WidthPx + x1) * 4u,
           (static_cast<size_t>(y1) * was.WidthPx + x0) * 4u,
           (static_cast<size_t>(y1) * was.WidthPx + x1) * 4u}};
}

inline float NearestToMean(std::span<const float, 4> sample) {
  const float mean = kOverFourSamples * (sample[0] + sample[1] + sample[2] + sample[3]);
  float best = sample[0];
  float distance = std::fabs(sample[0] - mean);
  for (int which = 1; which < 4; ++which) {
    const float other = std::fabs(sample[which] - mean);
    if (other < distance || (other == distance && sample[which] < best)) {
      best = sample[which];
      distance = other;
    }
  }
  return best;
}

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

inline Texels HalveInPlace(std::span<const float> from,
                           Texels was,
                           std::vector<float> &into,
                           TexelKind kind,
                           uint32_t indexChannels = 0) {
  const uint32_t fromWidth = was.WidthPx;
  const uint32_t fromHeight = was.HeightPx;
  const uint32_t toWidth = fromWidth > 1 ? fromWidth / 2u : 1u;
  const uint32_t toHeight = fromHeight > 1 ? fromHeight / 2u : 1u;
  into.assign(static_cast<size_t>(toWidth) * toHeight * 4u, 0.0f);
  for (uint32_t y = 0; y < toHeight; ++y) {
    for (uint32_t x = 0; x < toWidth; ++x) {
      const std::array<size_t, 4> source = FourUnder({.X = x, .Y = y}, was);
      const size_t at = (static_cast<size_t>(y) * toWidth + x) * 4u;
      for (size_t channel = 0; channel < 4; ++channel) {
        const std::array<float, 4> sample = {{from[source[0] + channel],
                                              from[source[1] + channel],
                                              from[source[2] + channel],
                                              from[source[3] + channel]}};
        into[at + channel] =
            ((indexChannels >> channel) & 1u) != 0u
                ? NearestToMean(sample)
                : kOverFourSamples * (sample[0] + sample[1] + sample[2] + sample[3]);
      }
      if (kind == TexelKind::Direction) {
        RenormaliseDirection(std::span<float, 4>(into.data() + at, 4));
      }
    }
  }
  return {.WidthPx = toWidth, .HeightPx = toHeight};
}

} // namespace outshine::Render

#endif
