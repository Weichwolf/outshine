/* THE RADIANCE COMPARISON, TAKEN IN THE TAP'S OWN ALPHABET. `doc/requirements.md` I.26.13: the
 * linear tap is `RGBA16Float`, binary16 carries an 11-bit significand, and at the values these cases
 * produce the storage quantisation is 63x the arithmetic term -- so a tolerance expressed in the
 * value's own units would be a statement about the format wearing the clothes of a threshold. The
 * bar is therefore exact and has nothing to nudge: **our stored half must be the nearest binary16 to
 * the oracle's f32**, and the residual is counted in ULPS OF BINARY16, which is a whole number.
 *
 * WHY NOT THE PNG. An 8-bit sRGB picture is a display encoding: two linear values a whole binade
 * apart can land on the same code, and two that agree to the last bit can straddle a code boundary
 * and differ by one. Both directions have already been measured on this corpus, so the number that
 * decides a radiance claim is read here and the picture is what a person looks at.
 *
 * ONLY WHERE THE ORACLE SAYS THE SUBJECT IS. A pixel the oracle left uncovered has no radiance to
 * compare, and including it would score the background's agreement with itself. */
#ifndef RENDER_RADIANCE_H
#define RENDER_RADIANCE_H

#include <cstdint>
#include <cstring>
#include <vector>

#include "OracleRaw.h"

namespace outshine::Render::Parity {

/* binary16 -> f32, by the format's own rules and not by a library: subnormals, infinities and NaN
 * all appear in a render target and a conversion that quietly flushed one would be a second thing
 * that can be wrong inside the instrument. */
[[nodiscard]] inline float FromHalf(uint16_t bits) {
  const uint32_t sign = static_cast<uint32_t>(bits & 0x8000u) << 16;
  uint32_t exponent = (bits >> 10) & 0x1Fu;
  uint32_t mantissa = bits & 0x3FFu;
  uint32_t assembled = 0;
  if (exponent == 0) {
    if (mantissa != 0) {
      /* Subnormal: renormalise until the implicit bit reappears. */
      int shift = 0;
      while ((mantissa & 0x400u) == 0) {
        mantissa <<= 1;
        ++shift;
      }
      mantissa &= 0x3FFu;
      assembled = ((127 - 15 - shift + 1) << 23) | (mantissa << 13);
    }
  } else if (exponent == 0x1Fu) {
    assembled = 0x7F800000u | (mantissa << 13);
  } else {
    assembled = ((exponent + 127 - 15) << 23) | (mantissa << 13);
  }
  assembled |= sign;
  float value = 0;
  std::memcpy(&value, &assembled, sizeof value);
  return value;
}

/* f32 -> the nearest binary16, round-half-to-even, which is what a render target's store does. */
[[nodiscard]] inline uint16_t ToHalf(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof bits);
  const uint16_t sign = static_cast<uint16_t>((bits >> 16) & 0x8000u);
  const int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xFFu) - 127 + 15;
  const uint32_t mantissa = bits & 0x7FFFFFu;
  if (((bits >> 23) & 0xFFu) == 0xFFu) {
    return static_cast<uint16_t>(sign | 0x7C00u | (mantissa != 0 ? 0x200u : 0u));
  }
  if (exponent >= 0x1F) { return static_cast<uint16_t>(sign | 0x7C00u); }
  if (exponent <= 0) {
    if (exponent < -10) { return sign; }
    const uint32_t whole = mantissa | 0x800000u;
    const int shift = 14 - exponent;
    const uint32_t rounded = (whole + (1u << (shift - 1)) - 1u +
                              ((whole >> shift) & 1u)) >> shift;
    return static_cast<uint16_t>(sign | rounded);
  }
  const uint32_t rounded =
      (mantissa + 0x0FFFu + ((mantissa >> 13) & 1u)) >> 13;
  return static_cast<uint16_t>(sign | ((static_cast<uint32_t>(exponent) << 10) + rounded));
}

/* The signed distance between two binary16 values counted in representable steps. Both are
 * non-negative radiances here, so the monotone integer ordering of the bit pattern IS the ordering
 * of the values and the difference of the patterns is the number of steps between them. */
[[nodiscard]] inline int UlpsApart(uint16_t ours, uint16_t theirs) {
  const int a = static_cast<int>(ours);
  const int b = static_cast<int>(theirs);
  return a > b ? a - b : b - a;
}

struct RadianceResidual {
  size_t Compared = 0;      /* channels, over the pixels the oracle covers */
  size_t BeyondNearest = 0; /* channels where our half is not the nearest one to the oracle's f32 */
  /* THE TWO POPULATIONS SEPARATED, because they lead to different work. One step is the last bit of
   * a conversion somewhere in the chain; more than one is a different value, and reporting only the
   * worst of them makes the next round measure the split again. */
  size_t BeyondOneUlp = 0;
  size_t BelowOracle = 0;   /* of the one-step ones, how many are LOW: a systematic bias shows here */
  int WorstUlps = 0;
  double WorstRelative = 0;
  double WorstOurs = 0, WorstTheirs = 0;
  size_t WorstX = 0, WorstY = 0;
  size_t WorstChannel = 0;
};

/* `linear` is the plan's `sceneLinear` readback, RGBA binary16, row-major, top row first. */
[[nodiscard]] inline RadianceResidual Radiance(const std::vector<uint16_t> &linear,
                                               const OracleRaw &oracle) {
  RadianceResidual out;
  const size_t width = static_cast<size_t>(oracle.Width());
  const size_t height = static_cast<size_t>(oracle.Height());
  if (linear.size() < width * height * 4u) { return out; }
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      if (!(oracle.At(static_cast<int>(x), static_cast<int>(y), 3) > 0.5f)) { continue; }
      for (size_t channel = 0; channel < 3; ++channel) {
        const uint16_t ours = linear[(y * width + x) * 4u + channel];
        const float theirs = oracle.At(static_cast<int>(x), static_cast<int>(y),
                                       static_cast<int>(channel));
        const uint16_t nearest = ToHalf(theirs);
        const int ulps = UlpsApart(ours, nearest);
        ++out.Compared;
        if (ulps == 0) { continue; }
        ++out.BeyondNearest;
        if (ulps > 1) { ++out.BeyondOneUlp; }
        if (ours < nearest) { ++out.BelowOracle; }
        if (ulps <= out.WorstUlps) { continue; }
        out.WorstUlps = ulps;
        out.WorstOurs = static_cast<double>(FromHalf(ours));
        out.WorstTheirs = static_cast<double>(theirs);
        out.WorstRelative = theirs != 0.0f
                                ? (out.WorstOurs - out.WorstTheirs) / static_cast<double>(theirs)
                                : 0.0;
        out.WorstX = x;
        out.WorstY = y;
        out.WorstChannel = channel;
      }
    }
  }
  return out;
}

} // namespace outshine::Render::Parity
#endif
