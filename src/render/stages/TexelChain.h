/* ONE MIP LEVEL FROM THE ONE ABOVE IT, BOX-FILTERED IN LINEAR LIGHT (board:1130).
 *
 * IT LIVES HERE RATHER THAN BESIDE ITS CALLER BECAUSE IT IS A PURE FUNCTION AND ITS CLAIM NEEDS A
 * TEST. Arrays in, arrays out, no device and no SDL -- so `test/unit/render/stages/` can hold it to
 * what it promises without a GPU. Left inside `SubjectDraw.cpp`'s anonymous namespace it was
 * unreachable, and the claim below would have been a comment rather than a check.
 *
 * LINEAR IS NOT A CHOICE HERE: `Upload` decodes sRGB before this runs, and averaging encoded values
 * would filter the transfer curve rather than the light. An odd dimension drops its last row or
 * column rather than weighting unevenly -- every corpus texture is a power of two, and a
 * non-power-of-two loses at most one texel of its smallest levels.
 *
 * A DIRECTION IS RENORMALISED AND A VALUE IS NOT, and that is the whole of what `TexelKind` buys.
 * It is an enumeration rather than a flag because four call sites each have to say which their
 * texels are, and a bool is four chances to say it wrong.
 *
 * WHY THE CHAIN IS BUILT HERE AND NOT BY THE DEVICE'S OWN GENERATOR: the generator has no way to be
 * told a texel is a direction, so it would average a normal map without renormalising -- a different
 * picture, arrived at silently, with no flag recording that it happened. */
#ifndef TEXEL_CHAIN_H
#define TEXEL_CHAIN_H

#include <cmath>
#include <cstdint>
#include <vector>

namespace outshine::Render {

enum class TexelKind { Value, Direction };

inline void HalveInPlace(const std::vector<float> &from, uint32_t fromWidth, uint32_t fromHeight,
                         std::vector<float> &into, uint32_t &toWidth, uint32_t &toHeight,
                         TexelKind kind) {
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
        into[at + channel] = 0.25f * (from[source[0] + channel] + from[source[1] + channel] +
                                      from[source[2] + channel] + from[source[3] + channel]);
      }
      if (kind != TexelKind::Direction) { continue; }
      /* THE MEAN OF UNIT VECTORS IS SHORT AND THE SHORTFALL IS LOST PERTURBATION. Renormalising
       * keeps the direction and DISCARDS that variance; carrying it instead as an increase in
       * roughness is Toksvig 2005 and LEAN/CLEAN (Real-Time Rendering 4e, ch. 9). It is not taken
       * here for a reason that is about the ORACLE rather than about cost: Cycles filters this
       * texture and normalises, and adding a roughness term the reference does not have would move
       * us away from the thing we are measured against to make a number smaller. The condition that
       * would justify it is specular aliasing visible IN MOTION, which is a scenario-suite finding
       * and cannot be seen in a still. */
      float direction[3];
      float length = 0.0f;
      for (int axis = 0; axis < 3; ++axis) {
        direction[axis] = into[at + (size_t)axis] * 2.0f - 1.0f;
        length += direction[axis] * direction[axis];
      }
      length = std::sqrt(length);
      if (length <= 0.0f) { continue; }
      for (int axis = 0; axis < 3; ++axis) {
        into[at + (size_t)axis] = (direction[axis] / length) * 0.5f + 0.5f;
      }
    }
  }
}

} // namespace outshine::Render

#endif
