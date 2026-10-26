#ifndef OUTSHINE_BASE_GEO_GEOCENTRIC_H
#define OUTSHINE_BASE_GEO_GEOCENTRIC_H

#include "Earth.h"
#include "Units.h"
#include "Vec3.h"
#include <cmath>
#include <limits>
#include <optional>

namespace outshine {
inline void GeoToEcef(const LongitudeLatitudeHeight &at, Vec3 &out) {
  const double latitude = at.LatitudeDeg * kDeg2Rad;
  const double longitude = at.LongitudeDeg * kDeg2Rad;
  const double sine = std::sin(latitude);
  const double cosine = std::cos(latitude);
  const double normal = kWgs84A / std::sqrt(1 - kWgs84E2 * sine * sine);
  out[0] = (normal + at.HeightM) * cosine * std::cos(longitude);
  out[1] = (normal + at.HeightM) * cosine * std::sin(longitude);
  out[2] = (normal * (1 - kWgs84E2) + at.HeightM) * sine;
}

[[nodiscard]] inline std::optional<LongitudeLatitudeHeight>
GeoFromEcef(const Vec3 &point) noexcept {
  const double horizontal = std::hypot(point[0], point[1]);
  const double radius = std::hypot(horizontal, point[2]);
  if (!std::isfinite(radius) || radius <= kWgs84A * kWgs84E2) { return std::nullopt; }
  double latitude = std::atan2(point[2], horizontal * (1 - kWgs84E2));
  constexpr int kMaximumPasses = 16;
  for (int pass = 0; pass < kMaximumPasses; ++pass) {
    const double sine = std::sin(latitude);
    const double cosine = std::cos(latitude);
    const double denominator = 1 - kWgs84E2 * sine * sine;
    const double normal = kWgs84A / std::sqrt(denominator);
    const double residual =
        horizontal * sine - point[2] * cosine - kWgs84E2 * normal * sine * cosine;
    const double derivative = horizontal * cosine + point[2] * sine -
                              kWgs84E2 * normal *
                                  (cosine * cosine - sine * sine +
                                   kWgs84E2 * sine * sine * cosine * cosine / denominator);
    const double correction = residual / derivative;
    if (!std::isfinite(correction)) { return std::nullopt; }
    const double next = latitude - correction;
    const bool converged = std::abs(correction) <= 2 * std::numeric_limits<double>::epsilon();
    latitude = next;
    if (!converged) { continue; }
    const double up = std::sin(latitude);
    const double height = horizontal * std::cos(latitude) + point[2] * up -
                          kWgs84A * std::sqrt(1 - kWgs84E2 * up * up);
    if (!std::isfinite(height)) { return std::nullopt; }
    return LongitudeLatitudeHeight{
        .LongitudeDeg = horizontal == 0 ? 0 : std::atan2(point[1], point[0]) / kDeg2Rad,
        .LatitudeDeg = latitude / kDeg2Rad,
        .HeightM = height};
  }
  return std::nullopt;
}
}
#endif
