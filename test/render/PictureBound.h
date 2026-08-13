/* THE PICTURE BOUND: every pixel of the frame, all four channels, on the case's own display transfer
 * (doc/requirements.md I.26.15).
 *
 * WHY IT EXISTS. `worst_disagreement_px` is a distance from a COVERAGE BOUNDARY, so interior noise
 * can never reach it: `water-bottle`'s reference was salt-and-pepper black dots and that case scored
 * 0, the best in the suite. The instrument was blind to the picture by CONSTRUCTION and not by
 * defect, and a claim about the picture needs an instrument whose domain is the picture.
 *
 * WHERE IT IS COMPUTED, and it is neither of the two obvious places. The linear f32 tap on both
 * sides, never the PNG -- a PNG carries a transfer AND a quantisation of its own, and that
 * quantisation is the very term this file has to derive. But the DIFFERENCE is taken on the display
 * axis, because a raw linear difference is the wrong unit in both directions: absolutely, a black
 * dot at 0.0 beside a neighbour at 0.04 is a tiny number and a glaring defect; relatively, 0 against
 * 1e-7 near black reads as infinite error and is invisible.
 *
 * ALPHA IS COMPARED AND IT IS NOT A COLOUR. Straight coverage on both sides, no transfer, because
 * encoding a coverage into a display code would bend it into something it is not. It is in the
 * comparison because without it a black subject and no subject are the same three channels: an empty
 * render scores 50 differing pixels under RGB alone against 46 151 under RGBA.
 *
 * THE BOUND IS A TAIL AND THE TAIL IS A SUM OF NAMED TERMS. `max delta_code <= N` gates; the count in
 * every bucket is published and unbounded. A count bound would have to be fitted to each case, which
 * I.26.12 forbids; a tail bound is a property of the pipeline that no case can move. The default is
 * ZERO and a term enters only where a mechanism puts it there. */
#ifndef RENDER_PICTUREBOUND_H
#define RENDER_PICTUREBOUND_H

#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "Mask.h"
#include "RawF32.h"
#include "SubTexelPrecision.h"

namespace outshine::Render::Parity {

/* THE DISPLAY TRANSFER, UNQUANTISED, and it is the whole of what `T` means here: the sRGB OETF over
 * scene-referred linear radiance, clamped to the display's own range because a code outside [0, 255]
 * is not a thing a picture can carry. Both sides of the comparison declare it -- our plan asks for
 * `Transfer::Linear` into an sRGB-encoding attachment, and the oracle's `viewTransform` is
 * `Standard`, which is the same curve. The clamp is deliberate and is the one thing this instrument
 * cannot see past: two radiances that both display as white ARE the same picture, and the radiance
 * residual on the linear tap is the instrument for the question this one stops asking. */
[[nodiscard]] inline double DisplayCode(double linear) {
  const double clamped = linear < 0.0 ? 0.0 : (linear > 1.0 ? 1.0 : linear);
  return clamped <= 0.0031308 ? 12.92 * clamped : 1.055 * std::pow(clamped, 1.0 / 2.4) - 0.055;
}

/* ONE TERM OF THE BOUND, WITH THE MECHANISM THAT PUT IT THERE. A bound that printed only its total
 * would be a chosen number wearing a derivation; printed term by term, a reader can see which
 * mechanism is spending it and a term whose mechanism goes away takes its codes with it. */
struct BoundTerm {
  std::string Mechanism;
  double Codes = 0;
};

/* WHAT IS IN THIS CASE'S PATH. Every field is read off the case rather than declared as a tolerance:
 * the estimator and the host residue come from the manifest's own description of its ORACLE, and the
 * sampler comes from whether a linearly filtered image is actually bound. There is no field here a
 * case can set to buy itself room. */
struct PathContents {
  /* Cycles still picks one source per shading event, so the reference is a draw rather than a value
   * and no tail bound against it is a bound on anything (I.26.15). */
  bool OracleEstimates = false;
  /* The host does not reproduce this subject's own render at a fixed seed. It is NOT an estimator --
   * there is nothing random left in the mathematics -- and it is not zero either. */
  bool OracleIsHostIrreproducible = false;
  double OracleHostResidueRelative = 0;
  /* A linearly filtered image is bound and sampled between texel centres. A nearest-filtered one
   * carries no weight and therefore no term. */
  bool LinearFilteredSampler = false;
};

/* THE BOUND, DERIVED. `Enforced` is false only where no tail bound may be enforced at all, which is
 * a different statement from a bound of zero and must never be printed as one. */
struct Tail {
  bool Enforced = true;
  double Codes = 0;
  std::vector<BoundTerm> Terms;
};

/* f32 ARITHMETIC ORDER, THE ONE TERM EVERY CASE CARRIES, AND IT IS NOT ZERO. Higham's
 * `gamma_n = n*u / (1 - n*u)` at `u = 2^-24` and the declared `n = 100` rounded operations
 * (I.26.13) is 5.96e-6 RELATIVE, and a relative residue costs `255 * sup[T'(x)*x]` codes -- so the
 * term is `255 * (1.055/2.4) * 5.96e-6 = 6.68e-4` codes.
 *
 * I.26.15's TABLE ROUNDS IT TO ZERO AND CALLS IT "below one part in 10^5 of a code". RECOMPUTED, it
 * is one part in 1 497, which is 67x that -- so the rounding to zero is an arithmetic slip and not a
 * derivation, and a bound of exactly zero is not the sum of a named term but a demand for
 * BIT-IDENTITY, which is a different claim that `linear_channels_differing` already makes. The term
 * is carried at its derived value. It is still 1 497x tighter than a single code, so nothing about
 * "almost identical" moves.
 *
 * MEASURED AGAINST IT: the seven flat cases sit at 2.70e-6 codes, every differing channel exactly
 * ONE f32 ulp, 9.15e-8 relative -- 65x inside the term. The term bounds the mechanism and the
 * mechanism is the one that is there. */
[[nodiscard]] inline double ArithmeticOrderCodes() {
  constexpr double kUnitRoundoff = 5.9604644775390625e-08; /* 2^-24, half an ulp at 1.0 */
  constexpr double kRoundedOperations = 100.0;             /* [SET] doc/requirements.md I.26.13 */
  constexpr double kWorstTransferGain = 1.055 / 2.4;
  const double relative =
      kRoundedOperations * kUnitRoundoff / (1.0 - kRoundedOperations * kUnitRoundoff);
  return 255.0 * kWorstTransferGain * relative;
}

/* THE SAMPLER'S WEIGHT TERM, AND THE DERIVATION IS NOT THE ONE I.26.15 SKETCHED. That sketch --
 * `255 * 2^-(n+1)`, half a division over a texel span of at most 255 codes -- is the term for a
 * sampler that interpolates in the ENCODED domain. Ours does not: `SubjectDraw::Upload` decodes
 * every sRGB image to linear f32 on the CPU and uploads `R32G32B32A32_FLOAT`, so the hardware
 * interpolates LINEAR radiance, which is also what Cycles does. The weight error is then
 * `|a - b| * 2^-(n+1)` in linear, and the display transfer is concave with `T(0) = 0`, so
 * `|T(x) - T(y)| <= T(|x - y|)` and the term is `255 * T(2^-(n+1))` at the worst adjacent pair --
 * two texels a full linear unit apart, sampled where the curve is steepest.
 *
 * THAT IS 6.43 CODES AT n = 8 AND NOT 0.50, and the difference is the sRGB curve's slope near black:
 * an error of 1/512 in linear IS six codes there. The looser number is the honest one for a linear
 * sampler, and the tighter one would have been a bound derived for a pipeline we do not have. */
[[nodiscard]] inline double SamplerWeightCodes() {
  return 255.0 * DisplayCode(1.0 / (2.0 * (double)outshine::Test::kSubTexelDivisions));
}

/* WHAT A RELATIVE RESIDUE IN THE ORACLE IS WORTH IN CODES. `sup over x in (0, 1] of T'(x) * x` is the
 * whole content: on the linear segment that product is `12.92 * x <= 0.04045`, and on the power
 * segment it is `(1.055 / 2.4) * x^(1/2.4)`, rising to `0.4395833...` at x = 1. So the sup is at
 * white and the term is `255 * 0.4395833 * residue`. */
[[nodiscard]] inline double HostResidueCodes(double relative) {
  constexpr double kWorstTransferGain = 1.055 / 2.4; /* derived: sup of T'(x)*x over (0,1], at x=1 */
  return 255.0 * kWorstTransferGain * relative;
}

[[nodiscard]] inline Tail BoundFor(const PathContents &path) {
  Tail tail;
  if (path.OracleEstimates) {
    tail.Enforced = false;
    return tail;
  }
  tail.Terms.push_back({"f32 arithmetic order", ArithmeticOrderCodes()});
  if (path.LinearFilteredSampler) {
    tail.Terms.push_back({"sub-texel weight snapping at 2^" +
                              std::to_string(outshine::Test::kSubTexelPrecisionBits) + " divisions",
                          SamplerWeightCodes()});
  }
  if (path.OracleIsHostIrreproducible) {
    tail.Terms.push_back({"the host's own residue between two oracle renders",
                          HostResidueCodes(path.OracleHostResidueRelative)});
  }
  for (const BoundTerm &term : tail.Terms) { tail.Codes += term.Codes; }
  return tail;
}

/* THE HISTOGRAM'S BUCKETS. One per whole code plus a final bucket for everything at or above 255,
 * which is a channel that agrees about nothing at all. */
constexpr size_t kCodeBuckets = 256;

struct PictureDelta {
  double MaxCode = 0;
  /* THE TAIL SPLIT BY WHAT IT IS ABOUT, because the two lead to different work and one number
   * carrying both cannot say which. A COLOUR difference is shading; an ALPHA difference is coverage,
   * and it is 0 or 255 by construction because both sides' alpha is a predicate -- so a single
   * silhouette pixel the two rasterisers place differently lands at 255 whatever the pictures look
   * like. The gate is the max of the two, as I.26.15 rules: whole image, every pixel, no mask. */
  double MaxColourCode = 0;
  double MaxAlphaCode = 0;
  size_t WorstX = 0, WorstY = 0;
  size_t WorstChannel = 0;
  double WorstOurs = 0, WorstTheirs = 0;
  size_t PixelsDiffering = 0;   /* any channel apart at all */
  size_t ChannelsCompared = 0;
  std::array<size_t, kCodeBuckets> Buckets{};
  bool Comparable = false;
};

/* THE FRAME THE BOUND IS COMPUTED ON, COMPOSED ONCE. `linear` is the plan's `sceneLinear` readback,
 * RGBA f32, top row first; its fourth channel is whatever the subject shader wrote and is NOT our
 * displayed alpha. Our displayed alpha is `covered(sceneDepth)`, which the display shader evaluates
 * off the depth attachment -- so it is taken from the depth mask here, the same expression over the
 * same input.
 *
 * IT IS COMPOSED RATHER THAN READ TWICE BECAUSE IT IS ALSO WHAT IS WRITTEN TO DISK. One buffer is
 * scored and stored, so "the float file beside the case is the frame the number came from" is a
 * property of the shape and not a claim anybody has to keep true. */
[[nodiscard]] inline std::vector<float> ScoredFrame(const std::vector<float> &linear,
                                                    const Mask &coverage) {
  const size_t pixels = (size_t)coverage.Width * (size_t)coverage.Height;
  if (linear.size() < pixels * 4u || coverage.In.size() < pixels) { return {}; }
  std::vector<float> frame(pixels * 4u);
  for (size_t pixel = 0; pixel < pixels; ++pixel) {
    for (size_t channel = 0; channel < 3; ++channel) {
      frame[pixel * 4u + channel] = linear[pixel * 4u + channel];
    }
    frame[pixel * 4u + 3u] = coverage.In[pixel] ? 1.0f : 0.0f;
  }
  return frame;
}

/* EVERY PIXEL, NO MASK. A mask here would be the previous instrument's blindness in a new place. */
[[nodiscard]] inline PictureDelta ComparePicture(const std::vector<float> &frame,
                                                 const RawF32 &oracle) {
  PictureDelta delta;
  const size_t width = (size_t)oracle.Width();
  const size_t height = (size_t)oracle.Height();
  if (frame.size() < width * height * 4u) { return delta; }
  delta.Comparable = true;
  const int alphaChannel = oracle.Channels() - 1;
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      bool apart = false;
      for (size_t channel = 0; channel < 4; ++channel) {
        const bool isAlpha = channel == 3;
        const double value = (double)frame[(y * width + x) * 4u + channel];
        const double ours = isAlpha ? value : DisplayCode(value);
        const double theirs =
            isAlpha ? (double)oracle.At((int)x, (int)y, alphaChannel)
                    : DisplayCode((double)oracle.At((int)x, (int)y, (int)channel));
        const double code = std::fabs(ours - theirs) * 255.0;
        ++delta.ChannelsCompared;
        if (code <= 0.0) { continue; }
        apart = true;
        size_t bucket = (size_t)code;
        if (bucket >= kCodeBuckets) { bucket = kCodeBuckets - 1; }
        ++delta.Buckets[bucket];
        double &worstOfItsKind = isAlpha ? delta.MaxAlphaCode : delta.MaxColourCode;
        if (code > worstOfItsKind) { worstOfItsKind = code; }
        if (code <= delta.MaxCode) { continue; }
        delta.MaxCode = code;
        delta.WorstX = x;
        delta.WorstY = y;
        delta.WorstChannel = channel;
        delta.WorstOurs = ours * 255.0;
        delta.WorstTheirs = theirs * 255.0;
      }
      delta.PixelsDiffering += apart ? 1u : 0u;
    }
  }
  return delta;
}

} // namespace outshine::Render::Parity
#endif
