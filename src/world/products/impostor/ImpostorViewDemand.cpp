#include "ImpostorViewDemand.h"
#include "Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

namespace outshine {
namespace {

struct DirectionScore {
  double Begin;
  double End;
  bool Elevated;
};

struct BestPair {
  double First = -std::numeric_limits<double>::infinity();
  double Second = First;

  void Add(double score) {
    if (score > First) {
      Second = std::exchange(First, score);
    } else if (score > Second) {
      Second = score;
    }
  }
};

std::vector<double> Crossings(std::span<const DirectionScore> scores) {
  std::vector<double> times{0, 1};
  for (size_t a = 0; a < scores.size(); ++a) {
    for (size_t b = a + 1; b < scores.size(); ++b) {
      const double begin = scores[a].Begin - scores[b].Begin;
      const double end = scores[a].End - scores[b].End;
      if ((begin < 0 && end > 0) || (begin > 0 && end < 0)) {
        times.push_back(begin / (begin - end));
      }
    }
  }
  std::ranges::sort(times);
  const auto duplicate = std::ranges::unique(times);
  times.erase(duplicate.begin(), duplicate.end());
  return times;
}

bool AdmitPair(std::span<const DirectionScore> scores,
               double at,
               double margin,
               bool requireElevatedView,
               std::vector<bool> &needed) {
  BestPair level;
  BestPair elevated;
  for (const auto &score : scores) {
    (score.Elevated ? elevated : level).Add(std::lerp(score.Begin, score.End, at));
  }
  if (requireElevatedView && elevated.First <= level.Second + margin) { return false; }
  const double second =
      std::max({level.Second, elevated.Second, std::min(level.First, elevated.First)});
  for (size_t i = 0; i < scores.size(); ++i) {
    const auto &score = scores[i];
    if (std::lerp(score.Begin, score.End, at) >= second - margin) { needed[i] = true; }
  }
  return true;
}

}

float ImpostorDistanceRamp::Weight(double distanceM) const {
  const double t = std::clamp((distanceM - BeginM) / (EndM - BeginM), 0.0, 1.0);
  return static_cast<float>(t * t * (3.0 - 2.0 * t));
}

ImpostorDistanceRamp ImpostorProjectionRamp(double halfExtentM,
                                            uint32_t side,
                                            double focalPixels,
                                            double maximumTexelPx) {
  constexpr double fadeRatio = 0.985;
  const double extent = std::numbers::sqrt3 * halfExtentM;
  const double distance = 2.0 * halfExtentM * focalPixels / (side * maximumTexelPx);
  return {.BeginM = extent + distance, .EndM = extent + distance / fadeRatio};
}

ImpostorDistanceMode ClassifyImpostorSweep(const ImpostorEyeSweep &eye,
                                           const ImpostorDistanceRamp &ramp) {
  if (std::max(Dot(eye.Begin, eye.Begin), Dot(eye.End, eye.End)) <= ramp.BeginM * ramp.BeginM) {
    return ImpostorDistanceMode::Geometry;
  }
  const auto segment = eye.End - eye.Begin;
  const double length2 = Dot(segment, segment);
  const double t = length2 > 0 ? std::clamp(-Dot(eye.Begin, segment) / length2, 0.0, 1.0) : 0;
  const auto nearest = eye.Begin + segment * t;
  return Dot(nearest, nearest) >= ramp.EndM * ramp.EndM ? ImpostorDistanceMode::Impostor
                                                        : ImpostorDistanceMode::Handoff;
}

std::vector<size_t> SelectImpostorDirections(std::span<const Vec3> directions,
                                             const ImpostorEyeSweep &eye,
                                             bool requireElevatedView) {
  std::vector<DirectionScore> scores;
  scores.reserve(directions.size());
  for (const auto &direction : directions) {
    scores.push_back({.Begin = Dot(direction, eye.Begin),
                      .End = Dot(direction, eye.End),
                      .Elevated = direction[1] > 0});
  }
  const auto times = Crossings(scores);
  const double margin = 1e-9 * std::max(Length(eye.Begin), Length(eye.End));
  std::vector<bool> needed(scores.size(), false);
  for (size_t i = 0; i < times.size(); ++i) {
    if (!AdmitPair(scores, times[i], margin, requireElevatedView, needed)) { return {}; }
    if (i > 0 &&
        !AdmitPair(scores, (times[i - 1] + times[i]) * 0.5, margin, requireElevatedView, needed)) {
      return {};
    }
  }
  std::vector<size_t> result;
  for (size_t at = 0; at < needed.size(); ++at) {
    if (needed[at]) { result.push_back(at); }
  }
  return result;
}

}
