#ifndef RENDER_RADIANCE_H
#define RENDER_RADIANCE_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "Metric.h"
#include "RawF32.h"

namespace outshine::Render::Parity {

[[nodiscard]] inline int64_t UlpsApart(float ours, float theirs) {
  uint32_t a = 0, b = 0;
  std::memcpy(&a, &ours, sizeof a);
  std::memcpy(&b, &theirs, sizeof b);
  const int64_t left = static_cast<int64_t>(a);
  const int64_t right = static_cast<int64_t>(b);
  return left > right ? left - right : right - left;
}

struct RadianceResidual {
  size_t Compared = 0;
  size_t Differing = 0;

  size_t BeyondOneUlp = 0;
  size_t BelowOracle = 0;
  int64_t WorstUlps = 0;
  double WorstRelative = 0;
  double WorstOurs = 0, WorstTheirs = 0;
  size_t WorstX = 0, WorstY = 0;
  size_t WorstChannel = 0;

  double P50Relative = 0, P95Relative = 0, P99Relative = 0;
};

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
        const float theirs =
            oracle.At(static_cast<int>(x), static_cast<int>(y), static_cast<int>(channel));
        ++out.Compared;
        const int64_t ulps = UlpsApart(ours, theirs);
        if (ulps == 0) { continue; }
        ++out.Differing;
        if (ulps > 1) { ++out.BeyondOneUlp; }
        if (ours < theirs) { ++out.BelowOracle; }
        const double apart = static_cast<double>(ours) - static_cast<double>(theirs);
        relative.push_back(
            theirs != 0.0f ? (apart < 0 ? -apart : apart) / static_cast<double>(theirs) : 0.0);
        if (ulps <= out.WorstUlps) { continue; }
        out.WorstUlps = ulps;
        out.WorstOurs = static_cast<double>(ours);
        out.WorstTheirs = static_cast<double>(theirs);
        out.WorstRelative =
            theirs != 0.0f ? (out.WorstOurs - out.WorstTheirs) / static_cast<double>(theirs) : 0.0;
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
