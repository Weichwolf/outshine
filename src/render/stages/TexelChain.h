#ifndef TEXEL_CHAIN_H
#define TEXEL_CHAIN_H

#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine::Render {

enum class TexelKind { Value, Direction };

inline uint32_t IndexChannelsOf(std::span<const float> texels) {
  uint32_t mask = 0;
  for (uint32_t channel = 0; channel < 4; ++channel) {
    float seen[2] = {0.0f, 0.0f};
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

inline void HalveInPlace(std::span<const float> from, uint32_t fromWidth, uint32_t fromHeight,
                         std::vector<float> &into, uint32_t &toWidth, uint32_t &toHeight,
                         TexelKind kind, uint32_t indexChannels = 0) {
  toWidth = fromWidth > 1 ? fromWidth / 2u : 1u;
  toHeight = fromHeight > 1 ? fromHeight / 2u : 1u;
  into.assign((size_t)toWidth * toHeight * 4u, 0.0f);
  for (uint32_t y = 0; y < toHeight; ++y) {
    for (uint32_t x = 0; x < toWidth; ++x) {
      const uint32_t x0 = fromWidth > 1 ? x * 2u : 0u, x1 = fromWidth > 1 ? x * 2u + 1u : 0u;
      const uint32_t y0 = fromHeight > 1 ? y * 2u : 0u, y1 = fromHeight > 1 ? y * 2u + 1u : 0u;
      const size_t source[4] = {((size_t)y0 * fromWidth + x0) * 4u,
                                ((size_t)y0 * fromWidth + x1) * 4u,
                                ((size_t)y1 * fromWidth + x0) * 4u,
                                ((size_t)y1 * fromWidth + x1) * 4u};
      const size_t at = ((size_t)y * toWidth + x) * 4u;
      for (size_t channel = 0; channel < 4; ++channel) {
        const float sample[4] = {from[source[0] + channel], from[source[1] + channel],
                                 from[source[2] + channel], from[source[3] + channel]};
        const float mean = 0.25f * (sample[0] + sample[1] + sample[2] + sample[3]);
        if (((indexChannels >> channel) & 1u) == 0u) {
          into[at + channel] = mean;
          continue;
        }

        float best = sample[0], distance = std::fabs(sample[0] - mean);
        for (int which = 1; which < 4; ++which) {
          const float other = std::fabs(sample[which] - mean);
          if (other < distance || (other == distance && sample[which] < best)) {
            best = sample[which];
            distance = other;
          }
        }
        into[at + channel] = best;
      }
      if (kind != TexelKind::Direction) { continue; }

      float direction[3];
      float length = 0.0f;
      for (int axis = 0; axis < 3; ++axis) {
        direction[axis] = into[at + (size_t)axis] * 2.0f - 1.0f;
        length += direction[axis] * direction[axis];
      }
      length = std::sqrt(length);
      into[at + 3u] *= length;
      if (length <= 0.0f) { continue; }
      for (int axis = 0; axis < 3; ++axis) {
        into[at + (size_t)axis] = (direction[axis] / length) * 0.5f + 0.5f;
      }
    }
  }
}

}

#endif
