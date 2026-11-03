#ifndef OUTSHINE_BASE_SPATIAL_GEODETICHEIGHT_H
#define OUTSHINE_BASE_SPATIAL_GEODETICHEIGHT_H

#include "Earth.h"
#include "Geocentric.h"
#include "TangentFrame.h"
#include "Vec2.h"
#include "Vec3.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <optional>
#include <numeric>

namespace outshine {
[[nodiscard]] inline std::optional<LongitudeLatitude> EllipsoidAt(const TangentFrame &frame,
                                                                  Vec2 xy) noexcept {
  const Vec3 base = frame.ToEcefPosition({.EastM = xy[0], .NorthM = xy[1]});
  const Vec3 &up = frame.UpEcef();
  constexpr Vec3 inverse{
      {1 / (kWgs84A * kWgs84A), 1 / (kWgs84A * kWgs84A), 1 / (kWgs84A * kWgs84A * (1 - kWgs84E2))}};
  double a = 0;
  double b = 0;
  double c = -1;
  for (size_t i = 0; i < inverse.Axis.size(); ++i) {
    a += up[i] * up[i] * inverse[i];
    b += base[i] * up[i] * inverse[i];
    c += base[i] * base[i] * inverse[i];
  }
  const double discriminant = b * b - a * c;
  if (!std::isfinite(discriminant) || discriminant < 0) { return std::nullopt; }
  const double denominator = b + std::sqrt(discriminant);
  if (!(denominator > 0)) { return std::nullopt; }
  const Vec3 surface = base + up * (-c / denominator);
  const double longitude = std::atan2(surface[1], surface[0]);
  const double latitude =
      std::atan2(surface[2] / (1 - kWgs84E2), std::hypot(surface[0], surface[1]));
  return LongitudeLatitude{.LongitudeDeg = longitude / kDeg2Rad,
                           .LatitudeDeg = latitude / kDeg2Rad};
}

namespace Detail {
template <typename HeightAt>
std::optional<double> BracketedRenderHeight(const TangentFrame &frame,
                                            Vec2 xy,
                                            std::array<double, 2> range,
                                            const HeightAt &heightAt) noexcept {
  const auto residual = [&](double up) -> std::optional<double> {
    const auto geo =
        GeoFromEcef(frame.ToEcefPosition({.EastM = xy[0], .NorthM = xy[1], .UpM = up}));
    if (!geo) { return std::nullopt; }
    const auto height =
        heightAt({.LongitudeDeg = geo->LongitudeDeg, .LatitudeDeg = geo->LatitudeDeg});
    return height && std::isfinite(*height) ? std::optional{geo->HeightM - *height} : std::nullopt;
  };
  const double tolerance = 4 * std::numeric_limits<float>::epsilon() *
                           std::max({1.0, std::abs(range[0]), std::abs(range[1])});
  range[0] -= tolerance;
  range[1] += tolerance;
  const auto low = residual(range[0]);
  const auto high = residual(range[1]);
  if (!low || !high || (*low < 0) == (*high < 0)) { return std::nullopt; }
  for (int pass = 0; pass < std::numeric_limits<double>::digits; ++pass) {
    const double up = std::midpoint(range[0], range[1]);
    const auto error = residual(up);
    if (!error) { return std::nullopt; }
    if (std::abs(*error) <= tolerance) { return up; }
    if (up == range[0] || up == range[1]) { return std::nullopt; }
    const size_t side = (*error < 0) == (*low < 0) ? 0 : 1;
    range[side] = up;
  }
  return std::nullopt;
}
}

template <typename HeightAt>
  requires requires(const HeightAt &source, LongitudeLatitude at) {
    { source(at) } noexcept -> std::same_as<std::optional<double>>;
  }
[[nodiscard]] std::optional<double> HeightInFrame(const TangentFrame &frame,
                                                  Vec2 xy,
                                                  double spacingM,
                                                  const HeightAt &heightAt) noexcept {
  if (!std::isfinite(xy[0]) || !std::isfinite(xy[1]) || !std::isfinite(spacingM) || spacingM <= 0) {
    return std::nullopt;
  }
  constexpr int kMaximumPasses = 16;
  constexpr double kEcefRoundoffM = 64 * std::numeric_limits<double>::epsilon() * kWgs84A;
  const double tolerance = kEcefRoundoffM + spacingM * std::numeric_limits<float>::epsilon();
  Vec2 query = xy;
  std::array<double, 2> range{std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity()};
  for (int pass = 0; pass < kMaximumPasses; ++pass) {
    const auto geo = EllipsoidAt(frame, query);
    if (!geo) { return std::nullopt; }
    const auto height = heightAt(*geo);
    if (!height || !std::isfinite(*height)) { return std::nullopt; }
    const auto point = frame.ToLocalPosition(
        {.LongitudeDeg = geo->LongitudeDeg, .LatitudeDeg = geo->LatitudeDeg, .HeightM = *height});
    const Vec2 residual{{xy[0] - point.EastM, xy[1] - point.NorthM}};
    if (!std::isfinite(point.UpM) || !std::isfinite(residual[0]) || !std::isfinite(residual[1])) {
      return std::nullopt;
    }
    if (std::max(std::abs(residual[0]), std::abs(residual[1])) <= tolerance) { return point.UpM; }
    range[0] = std::min(range[0], point.UpM);
    range[1] = std::max(range[1], point.UpM);
    query = query + residual;
  }
  return Detail::BracketedRenderHeight(frame, xy, range, heightAt);
}
}
#endif
