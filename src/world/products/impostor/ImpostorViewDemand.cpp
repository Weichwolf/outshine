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

constexpr double kCanopyScoreWidth = 0.025;
constexpr size_t kBlendCutIterations = 14;

struct DirectionScore {
  double Begin;
  double End;
  bool Elevated;
};

std::vector<DirectionScore> EyeDirectionScores(std::span<const Vec3> directions,
                                               const ImpostorEyeSweep &eye) {
  std::vector<DirectionScore> scores;
  scores.reserve(directions.size());
  for (const auto &direction : directions) {
    scores.push_back({.Begin = Dot(direction, eye.Begin),
                      .End = Dot(direction, eye.End),
                      .Elevated = direction[1] > 0});
  }
  return scores;
}

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

bool HasAngleSupport(const DirectionScore &view,
                     std::span<const DirectionScore> scores,
                     double limit,
                     std::vector<std::pair<double, int>> &crossings) {
  crossings.clear();
  int ahead = 0;
  for (const auto &other : scores) {
    const double gapBegin = other.Begin - view.Begin;
    const double gapEnd = other.End - view.End;
    const bool aheadAtBegin = gapBegin > limit;
    const bool aheadAtEnd = gapEnd > limit;
    ahead += static_cast<int>(aheadAtBegin);
    if (aheadAtBegin != aheadAtEnd) {
      crossings.emplace_back((limit - gapBegin) / (gapEnd - gapBegin), aheadAtEnd ? 1 : -1);
    }
  }
  if (ahead < 2) { return true; }
  std::ranges::sort(crossings);
  for (const auto &crossing : crossings) {
    ahead += crossing.second;
    if (ahead < 2) { return true; }
  }
  return false;
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
  const auto scores = EyeDirectionScores(directions, eye);
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

std::vector<size_t> SelectBlendedImpostorDirections(std::span<const Vec3> directions,
                                                    const ImpostorEyeSweep &eye) {
  const auto limit = (kCanopyScoreWidth + 1e-9) * std::max(Length(eye.Begin), Length(eye.End));
  const auto scores = EyeDirectionScores(directions, eye);
  std::vector<std::pair<double, int>> crossings;
  crossings.reserve(scores.size());
  std::vector<size_t> result;
  for (size_t at = 0; at < scores.size(); ++at) {
    if (HasAngleSupport(scores[at], scores, limit, crossings)) { result.push_back(at); }
  }
  return result;
}

float ImpostorDirectionWeight(double aboveCut) {
  return static_cast<float>(std::clamp(aboveCut / kCanopyScoreWidth, 0.0, 1.0));
}

double ImpostorDirectionCut(std::span<const double> scores) {
  BestPair best;
  for (const auto score : scores) { best.Add(score); }
  double upper = best.Second;
  double lower = upper - kCanopyScoreWidth;
  const auto totalWeight = [scores](double cut) {
    double weight = 0;
    for (const double score : scores) { weight += ImpostorDirectionWeight(score - cut); }
    return weight;
  };
  if (totalWeight(lower) <= 2.0) { return lower; }
  for (size_t step = 0; step < kBlendCutIterations; ++step) {
    const double cut = 0.5 * (lower + upper);
    if (totalWeight(cut) > 2.0) {
      lower = cut;
    } else {
      upper = cut;
    }
  }
  return 0.5 * (lower + upper);
}

}
