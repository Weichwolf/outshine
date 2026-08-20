#ifndef RENDER_METRIC_H
#define RENDER_METRIC_H

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace outshine::Render::Parity {

inline double Percentile(const std::vector<double> &sorted, double fraction) {
  if (sorted.empty()) { return 0.0; }
  size_t rank = (size_t)std::ceil(fraction * (double)sorted.size());
  if (rank == 0) { rank = 1; }
  if (rank > sorted.size()) { rank = sorted.size(); }
  return sorted[rank - 1];
}

enum class Direction { AtMost, AtLeast, Reported };

enum class Count { Criterion, Picture };

inline const char *Spelling(Direction direction) {
  switch (direction) {
  case Direction::AtMost: return "at most ";
  case Direction::AtLeast: return "at least";
  case Direction::Reported: return "reported";
  }
  return "?";
}

struct Metric {
  std::string Name;
  double Value = 0;
  double Threshold = 0;
  std::string Unit;
  Direction Against = Direction::Reported;
  Count Counts = Count::Criterion;

  [[nodiscard]] bool Held() const {
    switch (Against) {
    case Direction::AtMost: return Value <= Threshold;
    case Direction::AtLeast: return Value >= Threshold;
    case Direction::Reported: return true;
    }
    return false;
  }
};

inline void Print(const std::vector<Metric> &metrics) {
  for (const Metric &metric : metrics) {
    if (metric.Against == Direction::Reported) {
      std::printf("METRIC %-26s %14.8g %-13s  reported\n", metric.Name.c_str(), metric.Value,
                  metric.Unit.c_str());
      continue;
    }
    std::printf("METRIC %-26s %14.8g %-13s  %s %-14.8g  %s\n", metric.Name.c_str(), metric.Value,
                metric.Unit.c_str(), Spelling(metric.Against), metric.Threshold,
                metric.Held() ? "PASS" : "FAIL");
  }
}

}
#endif
