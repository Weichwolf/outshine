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

[[nodiscard]] inline double DisplayCode(double linear) {
  const double clamped = linear < 0.0 ? 0.0 : (linear > 1.0 ? 1.0 : linear);
  return clamped <= 0.0031308 ? 12.92 * clamped : 1.055 * std::pow(clamped, 1.0 / 2.4) - 0.055;
}

struct BoundTerm {
  std::string Mechanism;
  double Codes = 0;
};

struct PathContents {

  bool OracleEstimates = false;

  bool OracleIsHostIrreproducible = false;
  double OracleHostResidueRelative = 0;

  bool LinearFilteredSampler = false;
};

struct Tail {
  bool Enforced = true;
  double Codes = 0;
  std::vector<BoundTerm> Terms;
};

[[nodiscard]] inline double ArithmeticOrderCodes() {
  constexpr double kUnitRoundoff = 5.9604644775390625e-08;
  constexpr double kRoundedOperations = 100.0;
  constexpr double kWorstTransferGain = 1.055 / 2.4;
  const double relative =
      kRoundedOperations * kUnitRoundoff / (1.0 - kRoundedOperations * kUnitRoundoff);
  return 255.0 * kWorstTransferGain * relative;
}

[[nodiscard]] inline double SamplerWeightCodes() {
  return 255.0 * DisplayCode(1.0 / (2.0 * (double)outshine::Test::kSubTexelDivisions));
}

[[nodiscard]] inline double HostResidueCodes(double relative) {
  constexpr double kWorstTransferGain = 1.055 / 2.4;
  return 255.0 * kWorstTransferGain * relative;
}

inline constexpr double kPerceptualFloorCodes = 1.0;

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

constexpr size_t kCodeBuckets = 256;

struct Excursion {
  double Code = 0;
  size_t X = 0, Y = 0;
  size_t Channel = 0;
  double Ours = 0, Theirs = 0;
  size_t Pixels = 0;
};

struct PictureDelta {

  Excursion Appearance;

  Excursion Predicate;

  Excursion Routed;
  size_t PixelsDiffering = 0;
  size_t ChannelsCompared = 0;

  size_t OracleBlackChannels = 0;
  size_t OracleBlackWeLit = 0;
  double OracleBlackWorstCode = 0;

  std::array<Excursion, 8> Worst{};

  std::array<size_t, kCodeBuckets> Buckets{};
  bool Comparable = false;
};

[[nodiscard]] inline double PercentileCode(const std::array<size_t, kCodeBuckets> &buckets,
                                           size_t compared, double fraction) {
  if (compared == 0) { return 0.0; }
  const size_t want = (size_t)(fraction * (double)compared);

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

}

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

}
#endif
