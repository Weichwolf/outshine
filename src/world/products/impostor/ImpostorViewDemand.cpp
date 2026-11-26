#include "ImpostorViewDemand.h"
#include "Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace outshine {
namespace {

struct DirectionScore {
  double Begin;
  double End;
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

void AdmitPair(std::span<const DirectionScore> scores,
               double at,
               double margin,
               std::vector<bool> &needed) {
  double first = -std::numeric_limits<double>::infinity();
  double second = first;
  for (const auto &score : scores) {
    const double value = std::lerp(score.Begin, score.End, at);
    if (value > first) {
      second = std::exchange(first, value);
    } else if (value > second) {
      second = value;
    }
  }
  for (size_t i = 0; i < scores.size(); ++i) {
    const auto &score = scores[i];
    if (std::lerp(score.Begin, score.End, at) >= second - margin) { needed[i] = true; }
  }
}

}

std::vector<size_t> SelectImpostorDirections(std::span<const Vec3> directions,
                                             const ImpostorEyeSweep &eye) {
  std::vector<DirectionScore> scores;
  scores.reserve(directions.size());
  for (const auto &direction : directions) {
    scores.push_back({.Begin = Dot(direction, eye.Begin), .End = Dot(direction, eye.End)});
  }
  const auto times = Crossings(scores);
  const double margin = 1e-9 * std::max(Length(eye.Begin), Length(eye.End));
  std::vector<bool> needed(scores.size(), false);
  for (size_t i = 0; i < times.size(); ++i) {
    AdmitPair(scores, times[i], margin, needed);
    if (i > 0) { AdmitPair(scores, (times[i - 1] + times[i]) * 0.5, margin, needed); }
  }
  std::vector<size_t> result;
  for (size_t at = 0; at < needed.size(); ++at) {
    if (needed[at]) { result.push_back(at); }
  }
  return result;
}

}
