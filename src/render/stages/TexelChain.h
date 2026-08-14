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

/* WHICH CHANNELS ARE AN INDEX RATHER THAN A QUANTITY, ANSWERED FROM THE TEXELS AND NOT FROM THE SLOT.
 *
 * A channel taking AT MOST TWO DISTINCT VALUES carries a choice between two materials, not a measured
 * amount, and a box filter over it returns a value the asset does not contain. `normal-tangent`'s
 * metallic-roughness map is the case that named this: its metalness takes EXACTLY TWO values, 0 and
 * 255, while occlusion takes 85 and roughness 180 (board:1130). The whole-map mean of that metalness is
 * 86, so a fetch at the top of the chain returns 0.34 whether the texel under it was metal or
 * dielectric -- a third-metal, which is not a material and appears nowhere in the source.
 *
 * THE PREDICATE NEEDS NO THRESHOLD, which is why it is a count and not a histogram: 2 against 85 and
 * 180 has no midpoint to choose. EXACT float comparison is right here because a channel arrives as
 * `code / 255.0f` from an 8-bit source, so two texels of one code are bit-identical by construction.
 *
 * ITS DOMAIN IS TWO VALUES AND IT IS STATED RATHER THAN IMPLIED: a three-material index map reads as a
 * quantity here and is filtered as one. Widening it means deciding when a small distinct count stops
 * being an index, which is a threshold, and no case in this tree yet forces that choice. */
inline uint32_t IndexChannelsOf(const std::vector<float> &texels) {
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

inline void HalveInPlace(const std::vector<float> &from, uint32_t fromWidth, uint32_t fromHeight,
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
        /* AN INDEX CHANNEL SNAPS THE MEAN TO A VALUE THE FOUR ACTUALLY CONTAIN, so every level holds a
         * material the asset declares rather than an average of two. NEAREST-TO-THE-MEAN rather than
         * first-past-the-post because it does not depend on which corner a texel sits in -- a 2-2 split
         * must not resolve differently for a mirrored island than for its twin, and this tree has a
         * mirrored case whose whole purpose is to catch exactly that. The tie goes to the smaller value,
         * which is a declared choice and not an accident of iteration order. */
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
