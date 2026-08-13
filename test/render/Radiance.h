/* THE RADIANCE COMPARISON, IN THE TAP'S OWN ALPHABET (board:0087). Both sides are
 * f32: Cycles writes an f32 EXR and the plan declares `ScenePrecision::Float`, so `SceneLinear` is
 * an rgba32float attachment and the store rounds nothing. THE BAR IS THEREFORE THE VALUE ITSELF and
 * there is nothing in it to nudge -- our float either is the oracle's float or it is not.
 *
 * IT USED TO BE A BINARY16 QUESTION AND THAT WAS THE INSTRUMENT SPEAKING. At `RGBA16Float` the
 * nearest representable value to 0.0407008708 is 0.04071044921875, a relative error of 2.35e-4 --
 * 63x the worst-case arithmetic term over a hundred rounded operations -- and every channel of the
 * flat cases sat exactly one binary16 step low. That is a property of the format and not of anyone's
 * floating point, and I.26.13's rule is that the repair is the tap and never the threshold.
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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "Metric.h"
#include "RawF32.h"

namespace outshine::Render::Parity {

/* THE DISTANCE BETWEEN TWO f32 VALUES COUNTED IN REPRESENTABLE STEPS. Both are non-negative
 * radiances here, so the monotone integer ordering of the bit pattern IS the ordering of the values
 * and the difference of the patterns is the number of steps between them. Counted rather than
 * subtracted because a relative difference near the subnormal floor is not a comparable number:
 * binary16's floor is where 29 086 channels of `simple-texture` used to live. */
[[nodiscard]] inline int64_t UlpsApart(float ours, float theirs) {
  uint32_t a = 0, b = 0;
  std::memcpy(&a, &ours, sizeof a);
  std::memcpy(&b, &theirs, sizeof b);
  const int64_t left = static_cast<int64_t>(a);
  const int64_t right = static_cast<int64_t>(b);
  return left > right ? left - right : right - left;
}

struct RadianceResidual {
  size_t Compared = 0;      /* channels, over the pixels the oracle covers */
  size_t Differing = 0;     /* channels where our float is not the oracle's float */
  /* THE TWO POPULATIONS SEPARATED, because they lead to different work. One step is the last bit of
   * a rounding somewhere in the chain; more than one is a different value, and reporting only the
   * worst of them makes the next round measure the split again. */
  size_t BeyondOneUlp = 0;
  size_t BelowOracle = 0;   /* how many of the differing ones are LOW: a systematic bias shows here */
  int64_t WorstUlps = 0;
  double WorstRelative = 0;
  double WorstOurs = 0, WorstTheirs = 0;
  size_t WorstX = 0, WorstY = 0;
  size_t WorstChannel = 0;
  /* THE RESIDUAL AS A FRACTION OF THE VALUE, over the channels that differ. A ulp count says how
   * many representable steps apart two numbers are and says nothing about how much that is; near
   * zero a million ulps is nothing and near one it is everything. Reported as a distribution because
   * a worst case over 10^5 channels is one draw (`CLAUDE.md`). */
  double P50Relative = 0, P95Relative = 0, P99Relative = 0;
};

/* `linear` is the plan's `sceneLinear` readback, RGBA f32, row-major, top row first. */
[[nodiscard]] inline RadianceResidual Radiance(const std::vector<float> &linear,
                                               const RawF32 &oracle) {
  RadianceResidual out;
  std::vector<double> relative;
  const size_t width = static_cast<size_t>(oracle.Width());
  const size_t height = static_cast<size_t>(oracle.Height());
  if (linear.size() < width * height * 4u) { return out; }
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      if (!(oracle.At(static_cast<int>(x), static_cast<int>(y), 3) > 0.5f)) { continue; }
      for (size_t channel = 0; channel < 3; ++channel) {
        const float ours = linear[(y * width + x) * 4u + channel];
        const float theirs = oracle.At(static_cast<int>(x), static_cast<int>(y),
                                       static_cast<int>(channel));
        ++out.Compared;
        const int64_t ulps = UlpsApart(ours, theirs);
        if (ulps == 0) { continue; }
        ++out.Differing;
        if (ulps > 1) { ++out.BeyondOneUlp; }
        if (ours < theirs) { ++out.BelowOracle; }
        const double apart = static_cast<double>(ours) - static_cast<double>(theirs);
        relative.push_back(theirs != 0.0f ? (apart < 0 ? -apart : apart) / static_cast<double>(theirs)
                                          : 0.0);
        if (ulps <= out.WorstUlps) { continue; }
        out.WorstUlps = ulps;
        out.WorstOurs = static_cast<double>(ours);
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
  std::sort(relative.begin(), relative.end());
  out.P50Relative = Percentile(relative, 0.50);
  out.P95Relative = Percentile(relative, 0.95);
  out.P99Relative = Percentile(relative, 0.99);
  return out;
}

} // namespace outshine::Render::Parity
#endif
