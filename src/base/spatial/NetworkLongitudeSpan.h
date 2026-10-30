#ifndef OUTSHINE_BASE_SPATIAL_NETWORKLONGITUDESPAN_H
#define OUTSHINE_BASE_SPATIAL_NETWORKLONGITUDESPAN_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace outshine::Path {
struct LongitudeColumn {
  double Degrees = 0;
  int64_t Index = 0;
};

// Inclusive unwrapped columns; callers reduce modulo columns, visiting each cell once.
[[nodiscard]] inline std::pair<int64_t, int64_t>
ColumnsAlongShortestArc(LongitudeColumn from, LongitudeColumn to, int64_t columns) noexcept {
  constexpr double kFullTurnDeg = 360.0;
  constexpr double kHalfTurnDeg = kFullTurnDeg / 2;
  const double difference = to.Degrees - from.Degrees;
  if (from.Degrees >= -kHalfTurnDeg && from.Degrees < kHalfTurnDeg && to.Degrees >= -kHalfTurnDeg &&
      to.Degrees < kHalfTurnDeg && difference > -kHalfTurnDeg && difference < kHalfTurnDeg) {
    return {std::min(from.Index, to.Index), std::max(from.Index, to.Index)};
  }
  const double origin =
      from.Degrees - kFullTurnDeg * std::floor((from.Degrees + kHalfTurnDeg) / kFullTurnDeg);
  const double run = difference - kFullTurnDeg * std::floor(difference / kFullTurnDeg + 0.5);
  const bool reverse = run < 0.0;
  const int64_t first = reverse ? to.Index : from.Index;
  int64_t last = reverse ? from.Index : to.Index;
  const bool wraps = reverse ? origin + run < -kHalfTurnDeg : origin + run >= kHalfTurnDeg;
  if (wraps || last < first) { last += columns; }
  return {first, std::min(last, first + columns - 1)};
}
}

#endif
