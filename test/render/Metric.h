/* WHAT A RENDER CASE'S VERDICT IS MADE OF: named metrics, each carrying its own threshold and its
 * own DIRECTION, and pass is every one of them within its own (doc/requirements.md I.26.10).
 *
 * DIRECTION IS AN ENUMERATION AND NEVER A FLAG (`Enum.2`). `lowerIsBetter` as a bool is the mistake
 * that inverts a whole suite silently while every test still reports green, and a bool has no way to
 * say which of its two values is which at the call site. `AtMost` and `AtLeast` do.
 *
 * A METRIC MAY ALSO BE REPORTED RATHER THAN ENFORCED, and that is a third state of the same object
 * rather than a second kind of thing -- IoU is published beside the boundary distribution and decides
 * nothing (I.26), so `Reported` exists here and `iou` simply carries it. */
#ifndef RENDER_METRIC_H
#define RENDER_METRIC_H

#include <cstdio>
#include <string>
#include <vector>

namespace outshine::Render::Parity {

enum class Direction { AtMost, AtLeast, Reported };

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

  [[nodiscard]] bool Held() const {
    switch (Against) {
    case Direction::AtMost: return Value <= Threshold;
    case Direction::AtLeast: return Value >= Threshold;
    case Direction::Reported: return true;
    }
    return false;
  }
};

/* EVERY METRIC IS PRINTED, not only the failing one, so a case that passes narrowly is visible
 * before it starts failing. */
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

} // namespace outshine::Render::Parity
#endif
