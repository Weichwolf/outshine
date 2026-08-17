/* THE PICTURE BOUND: every pixel of the frame, all four channels, on the case's own display transfer
 * (board:0089).
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
 * EVERY PIXEL IS GATED BY EXACTLY ONE BOUND AND THE ROUTING IS BY WHAT KIND OF QUANTITY IT CARRIES
 * (I.26.15). Where the two sides agree about covering a pixel AND about which surface covers it, its
 * colour is appearance and it goes to the perceptual tail below. Where either agreement fails, its
 * difference is geometric and expressed in the wrong unit: both sides sample once, ours at the pixel
 * centre and Cycles through a 0.01 px box, so a 0.0013 px tie becomes 187 codes because coverage has
 * two values -- the amplification is the instrument's and not the picture's. That pixel is routed to
 * I.26.14, which is STRICTER: it demands the edge sit within the oracle's own filter half-width. The
 * pixel is routed, never excluded, and its count and worst code are published beside the tail.
 *
 * THE SECOND AGREEMENT IS THE LATER ONE AND IT IS `Routing.h`'s (board:1144). Agreement about
 * coverage is not agreement about what is there: two sides can both cover a pixel and draw two
 * different materials at it, and scored perceptually that lands as a colour disagreement between two
 * surfaces nobody claimed were the same one.
 *
 * WHAT CAN HIDE IN THE GAP: nothing. A defect that moves a silhouette visibly exceeds the filter
 * half-width and I.26.14 refuses it; a defect that changes a covered pixel's colour is in the tail
 * here. What remains between them is a geometric difference below the oracle's own filter, where the
 * reference cannot say which side of the edge it is on either.
 *
 * ALPHA IS A PREDICATE AND HAS NO PERCEPTUAL AXIS, so it is never in the tail: `T` is a transfer for
 * RADIANCE, and applying it to a coverage decision produces a number with no meaning. Where the two
 * coverage masks agree, alpha must agree EXACTLY -- zero is not a tolerance here but the statement
 * that a predicate has one value -- and a fractional oracle alpha over our `covered(sceneDepth)` is
 * a defect this gate is meant to reach rather than absorb. Where they disagree, alpha is routed with
 * the rest of the pixel.
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
#include "Routing.h"
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
  constexpr double kRoundedOperations = 100.0;             /* [SET] board:0087 */
  constexpr double kWorstTransferGain = 1.055 / 2.4;
  const double relative =
      kRoundedOperations * kUnitRoundoff / (1.0 - kRoundedOperations * kUnitRoundoff);
  return 255.0 * kWorstTransferGain * relative;
}

/* THE SAMPLER'S WEIGHT TERM, AND THE DERIVATION IS NOT THE ONE I.26.15 SKETCHED. That sketch --
 * `255 * 2^-(n+1)`, half a division over a texel span of at most 255 codes -- is the term for a
 * sampler that interpolates in the ENCODED domain. Ours does not: `SubjectDraw::Upload` decodes
 * every sRGB image to linear f32 on the CPU and uploads `R32G32B32A32_FLOAT`, so the hardware
 * interpolates LINEAR radiance, which is also what Cycles does. The weight error is `|a - b| * e` in
 * linear, and the display transfer is concave with `T(0) = 0`, so `|T(x) - T(y)| <= T(|x - y|)` and
 * the term is `255 * T(e)` at the worst adjacent pair -- two texels a full linear unit apart,
 * sampled where the curve is steepest.
 *
 * `e` IS HALF A DIVISION HERE AND THE TERM IS 6.4348 CODES AT n = 8 -- and `texture-coordinate-test`
 * EXCEEDS IT AT 10.295625 codes, which this file states rather than accommodates. What that residual
 * is has been measured and it is NOT a defect in either sampler:
 *
 *   - both sides snap the weight to the SAME lattice. Over `texture-coordinate-test`'s checker
 *     strip, 135 of 217 oracle samples land exactly on multiples of `0.8/256` -- the white texel
 *     times the material's 0.8 albedo, over 256 divisions -- which a continuous interpolator hits
 *     with probability zero. Cycles quantises too.
 *   - so the disagreement is the gap between two independent roundings to one lattice, which is a
 *     WHOLE division per axis and not half of one: each rounding is within half a step of the true
 *     weight, so the two can straddle a boundary. Measured over that strip: 73 px at -1 step, 84 at
 *     +1, and 2 px at +/-2 -- and both of the two-step pixels lie where BOTH axes interpolate, which
 *     is one step per axis and is what a bilinear sampler with two snapped weights predicts.
 *   - `255 * 12.92 * 0.8/256 = 10.295625` codes is therefore the PREDICTED worst pixel of this case
 *     from `n`, the albedo and the transfer alone, and the measured worst pixel is 10.295625.
 *
 * THE TERM IS LEFT AT HALF A DIVISION ALL THE SAME, and that is a decision rather than an oversight.
 * Carrying the derivation to its end gives `255 * T(2/256) = 21.60` codes at a full-range span --
 * 3.4x this term -- and a ceiling that wide admits `scifi-helmet` at 15.46 codes, which has no
 * measured connection to this mechanism at all. A term is only worth its width where the mechanism
 * is shown to be in the path, and widening one term to rescue a case it was not measured on is the
 * move I.26.15 exists to prevent. The residual is attributed and published; moving the ceiling is
 * the architect's (board:0089's own table carries 6.4348).
 *
 * THE DIVISION COUNT IS MEASURED AT THE CORPUS'S OWN TEXTURE WIDTH and not only at two texels
 * (`test/shader/TheSamplerSnapsSubTexelWeightsToTheDeclaredCount.cpp`): 512 texels returns the same
 * 257 endpoints at the same 1/256 step, so `n` does not depend on the width and the hypothesis that
 * it did is refuted rather than assumed away. */
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

/* [DERIVED] ONE CODE OF THE DECLARED 8-BIT TRANSFER (board:1359, board:1367). Below the transfer's own
 * quantisation step, "the two pictures differ" is not a representable claim -- no display the picture is
 * judged on can show it. Not two, which would be a comfort margin, and a derivation may not contain
 * comfort; not zero either, because a sum of instrument terms can land four orders under a code and a
 * bound that tight is a demand for bit-identity wearing a perceptual bound's name, which
 * `linear_channels_differing` already makes as its own claim. */
inline constexpr double kPerceptualFloorCodes = 1.0;

/* [SET BY THE OWNER] THE FRACTION OF CHANNELS THE BOUND IS ASKED ABOUT. The metric was a MAXIMUM over
 * 1280 x 720 x 3 = 2 764 800 channels -- a claim about the single worst channel, where the finish line
 * is a claim about a PICTURE. `WaterBottle` is the case that made it visible: its two renders are
 * indistinguishable by eye and it scored 149.27 codes and counted as outside. The owner's tolerance is
 * that the pictures look 99 % alike, so 99 % is what the bound is applied to and the maximum keeps its
 * own row. */
inline constexpr double kBoundFraction = 0.99;

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
  if (tail.Codes < kPerceptualFloorCodes) {
    tail.Terms.push_back({"the 8-bit transfer's own quantisation step",
                          kPerceptualFloorCodes - tail.Codes});
    tail.Codes = kPerceptualFloorCodes;
  }
  return tail;
}



/* THE HISTOGRAM'S BUCKETS. One per whole code plus a final bucket for everything at or above 255,
 * which is a channel that agrees about nothing at all. */
constexpr size_t kCodeBuckets = 256;

/* THE WORST CHANNEL OF ONE KIND, WITH WHERE IT WAS AND WHAT THE TWO SIDES SAID THERE. Three of these
 * make a comparison, and a caller cannot mix them up: each is reached by a name that says which
 * quantity it bounds, and none of them holds a maximum over a population it does not name. */
struct Excursion {
  double Code = 0;
  size_t X = 0, Y = 0;
  size_t Channel = 0;
  double Ours = 0, Theirs = 0;
  size_t Pixels = 0; /* pixels of this kind with any channel of this kind apart */
};

struct PictureDelta {
  /* Colour, where the two sides agree about coverage. THIS IS THE GATED TAIL. */
  Excursion Appearance;
  /* Alpha, where the two sides agree about coverage. Gated at zero: a predicate has one value. */
  Excursion Predicate;
  /* Every channel, at the pixels the two sides disagree about covering. Published here and gated by
   * I.26.14's `worst_disagreement_px`, which is a distance in pixels rather than a code. */
  Excursion Routed;
  size_t PixelsDiffering = 0; /* any channel apart at all, RGBA, whole image */
  size_t ChannelsCompared = 0;
  /* THE ORACLE'S EXACT ZEROS, AND WHAT WE PUT THERE. Eight of the thirteen cases outside the bound have
   * a worst pixel of the same shape -- `ours <something> against 0` -- and a single worst pixel is an
   * anecdote until it is counted. `Zero` here is EXACT and on the LINEAR radiance, not a small code:
   * Cycles returning exactly 0 is a statement that no light path reached the pixel, which is a
   * different fact from a dark one. Restricted to channels where the two sides agree the pixel is
   * covered, or the background would swamp it. */
  size_t OracleBlackChannels = 0;    /* covered-both channels where the oracle is exactly 0 */
  size_t OracleBlackWeLit = 0;       /* of those, the ones where we are not */
  double OracleBlackWorstCode = 0;   /* the worst code among them */
  /* THE CHANNELS THAT DECIDE THE CASE, not just the one that wins. The tail is a MAX, so a picture
   * exact everywhere else fails on whatever handful is worst -- `texture-coordinate-test` has FOUR
   * channels above 5 codes in 2 672 379, and one worst pixel cannot say whether they share a cause or
   * a place. Eight is enough to see a pattern and small enough to print in every case; how many were
   * left out is published beside them, because a silent cap reads as "that was all of them". */
  std::array<Excursion, 8> Worst{};
  /* Of the tail only, because the tail is what the histogram is the shape of. A routed channel's
   * code is an artefact of binary coverage and would pile up at 255 saying nothing. */
  std::array<size_t, kCodeBuckets> Buckets{};
  bool Comparable = false;
};

/* THE CODE VALUE BELOW WHICH `fraction` OF THE COMPARED CHANNELS LIE, read off the tail's own histogram.
 * The buckets are one code wide, so this answers to within a code -- which is exactly the resolution the
 * perceptual floor makes meaningful, and no finer claim is made from it. A channel in bucket n differed
 * by at least n and less than n+1, so the value returned is the bucket's UPPER edge: the smallest whole
 * code that bounds that fraction of the population. */
[[nodiscard]] inline double PercentileCode(const std::array<size_t, kCodeBuckets> &buckets,
                                           size_t compared, double fraction) {
  if (compared == 0) { return 0.0; }
  const size_t want = (size_t)(fraction * (double)compared);
  /* THE HISTOGRAM IS OF THE TAIL AND THE POPULATION IS EVERY COMPARED CHANNEL. Channels the two sides
   * agree about exactly are not counted into a bucket, so they are counted in HERE, at zero -- and
   * leaving them out was a defect that put every case outside its bound: the buckets alone never reach
   * 99 % of the compared channels, so the walk fell off the end and returned the ceiling. */
  size_t counted = 0;
  for (size_t bucket = 0; bucket < kCodeBuckets; ++bucket) { counted += buckets[bucket]; }
  size_t seen = compared > counted ? compared - counted : 0;
  if (seen >= want) { return 0.0; }
  for (size_t bucket = 0; bucket < kCodeBuckets; ++bucket) {
    seen += buckets[bucket];
    if (seen >= want) { return (double)(bucket + 1); }
  }
  return (double)kCodeBuckets;
}

namespace Detail {

inline void Widen(Excursion &worst, double code, size_t x, size_t y, size_t channel, double ours,
                  double theirs) {
  if (code <= worst.Code) { return; }
  worst = {code, x, y, channel, ours, theirs, worst.Pixels};
}

} // namespace Detail

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

/* EVERY PIXEL, NO MASK -- and the router is a router rather than a filter: every channel of every
 * pixel lands in exactly one of the three excursions and none is dropped. The masks are passed in
 * because the predicate `is this pixel covered` has one spelling per side already (`FromDepth` and
 * `FromOracle`), and rewriting either here would be a second one that can drift; the routing rule
 * itself lives in `Routing.h` for the same reason, since the geometric bound has to walk the same
 * population this loop sends to it. */
[[nodiscard]] inline PictureDelta ComparePicture(const std::vector<float> &frame,
                                                 const RawF32 &oracle, const Routing &routing) {
  const Mask &ours = routing.Ours;
  const Mask &theirs = routing.Theirs;
  PictureDelta delta;
  const size_t width = (size_t)oracle.Width();
  const size_t height = (size_t)oracle.Height();
  if (frame.size() < width * height * 4u || ours.Width != oracle.Width() ||
      theirs.Width != oracle.Width() || ours.Height != oracle.Height() ||
      theirs.Height != oracle.Height()) {
    return delta;
  }
  delta.Comparable = true;
  const int alphaChannel = oracle.Channels() - 1;
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      const bool toAppearance = routing.ToAppearance((int)x, (int)y);
      /* THE BLACK DIAGNOSTIC BELOW KEEPS THE COVERAGE POPULATION AND NOT THE ROUTED ONE, and the two
       * stopped being the same question when the router grew its third mask (board:1144): "how often
       * is the oracle black where both sides drew something" is about what the two sides COVER, and
       * narrowing it to what they also agree about the identity of would move a published number by
       * changing its selection. */
      const bool bothCover = ours.At((int)x, (int)y) == theirs.At((int)x, (int)y);
      bool apart = false, routedApart = false, appearanceApart = false, predicateApart = false;
      for (size_t channel = 0; channel < 4; ++channel) {
        const bool isAlpha = channel == 3;
        const double value = (double)frame[(y * width + x) * 4u + channel];
        const double oursCode = isAlpha ? value : DisplayCode(value);
        const double theirsCode =
            isAlpha ? (double)oracle.At((int)x, (int)y, alphaChannel)
                    : DisplayCode((double)oracle.At((int)x, (int)y, (int)channel));
        const double code = std::fabs(oursCode - theirsCode) * 255.0;
        ++delta.ChannelsCompared;
        /* COUNTED BEFORE THE EARLY EXIT, because the population includes the channels that AGREE: a
         * count of disagreements over a population of disagreements says nothing about how often the
         * oracle is black at all, and that ratio is the finding. */
        if (bothCover && !isAlpha && (double)oracle.At((int)x, (int)y, (int)channel) == 0.0) {
          ++delta.OracleBlackChannels;
          if (value > 0.0f) {
            ++delta.OracleBlackWeLit;
            if (code > delta.OracleBlackWorstCode) { delta.OracleBlackWorstCode = code; }
          }
        }
        if (code <= 0.0) { continue; }
        apart = true;
        if (!toAppearance) {
          routedApart = true;
          Detail::Widen(delta.Routed, code, x, y, channel, oursCode * 255.0, theirsCode * 255.0);
          continue;
        }
        if (isAlpha) {
          predicateApart = true;
          Detail::Widen(delta.Predicate, code, x, y, channel, oursCode * 255.0, theirsCode * 255.0);
          continue;
        }
        appearanceApart = true;
        size_t bucket = (size_t)code;
        if (bucket >= kCodeBuckets) { bucket = kCodeBuckets - 1; }
        ++delta.Buckets[bucket];
        Detail::Widen(delta.Appearance, code, x, y, channel, oursCode * 255.0, theirsCode * 255.0);
        /* Insertion into a table of eight, kept descending. Linear because eight is eight. */
        if (code > delta.Worst[delta.Worst.size() - 1u].Code) {
          size_t at = delta.Worst.size() - 1u;
          while (at > 0 && code > delta.Worst[at - 1u].Code) {
            delta.Worst[at] = delta.Worst[at - 1u];
            --at;
          }
          delta.Worst[at] = Excursion{code,          x, y, channel, oursCode * 255.0,
                                      theirsCode * 255.0, 0};
        }
      }
      delta.PixelsDiffering += apart ? 1u : 0u;
      delta.Routed.Pixels += routedApart ? 1u : 0u;
      delta.Appearance.Pixels += appearanceApart ? 1u : 0u;
      delta.Predicate.Pixels += predicateApart ? 1u : 0u;
    }
  }
  return delta;
}

} // namespace outshine::Render::Parity
#endif
