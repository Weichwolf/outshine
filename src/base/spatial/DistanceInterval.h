#ifndef OUTSHINE_BASE_SPATIAL_DISTANCEINTERVAL_H
#define OUTSHINE_BASE_SPATIAL_DISTANCEINTERVAL_H
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace outshine::DistanceArithmetic {
static_assert(std::numeric_limits<double>::is_iec559);

struct Interval {
  double Lower;
  double Upper;
};

[[nodiscard]] inline Interval Enclose(double lower, double upper) noexcept {
  return {.Lower = std::nextafter(lower, -std::numeric_limits<double>::infinity()),
          .Upper = std::nextafter(upper, std::numeric_limits<double>::infinity())};
}

[[nodiscard]] inline Interval Add(Interval a, Interval b) noexcept {
  return Enclose(a.Lower + b.Lower, a.Upper + b.Upper);
}

[[nodiscard]] inline Interval Subtract(Interval a, Interval b) noexcept {
  return Enclose(a.Lower - b.Upper, a.Upper - b.Lower);
}

[[nodiscard]] inline Interval Multiply(Interval a, Interval b) noexcept {
  const std::array products{
      a.Lower * b.Lower, a.Lower * b.Upper, a.Upper * b.Lower, a.Upper * b.Upper};
  return Enclose(*std::ranges::min_element(products), *std::ranges::max_element(products));
}

[[nodiscard]] inline double UpperNorm(const std::array<Interval, 3> &components) noexcept {
  Interval squared{.Lower = 0, .Upper = 0};
  for (const Interval component : components) {
    const double extent = std::max(std::abs(component.Lower), std::abs(component.Upper));
    squared = Add(squared,
                  Multiply({.Lower = extent, .Upper = extent}, {.Lower = extent, .Upper = extent}));
  }
  return std::nextafter(std::sqrt(squared.Upper), std::numeric_limits<double>::infinity());
}
}
#endif
