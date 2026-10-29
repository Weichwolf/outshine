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
  const double origin = from.Degrees - 360.0 * std::floor((from.Degrees + 180.0) / 360.0);
  const double difference = to.Degrees - from.Degrees;
  const double run = difference - 360.0 * std::floor(difference / 360.0 + 0.5);
  const bool reverse = run < 0.0;
  const int64_t first = reverse ? to.Index : from.Index;
  int64_t last = reverse ? from.Index : to.Index;
  const bool wraps = reverse ? origin + run < -180.0 : origin + run >= 180.0;
  if (wraps || last < first) { last += columns; }
  return {first, std::min(last, first + columns - 1)};
}
}

#endif
