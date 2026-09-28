#include "TriangleRegion.h"
#include "DistanceInterval.h"
#include "TriangleDistance.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace outshine {
namespace {
using DistanceArithmetic::Add;
using DistanceArithmetic::Interval;
using DistanceArithmetic::Multiply;
using DistanceArithmetic::Subtract;

[[nodiscard]] bool ValidPoint(const PointEnclosure &point) noexcept {
  if (!(point.RadiusM >= 0) || !std::isfinite(point.RadiusM)) { return false; }
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(point.EstimateM[axis])) { return false; }
  }
  return true;
}
}

std::optional<PointEnclosure> EnclosePointBlend(const std::array<PointEnclosure, 2> &endpoints,
                                                double towardEnd) noexcept {
  if (!ValidPoint(endpoints[0]) || !ValidPoint(endpoints[1]) || !std::isfinite(towardEnd) ||
      towardEnd < 0 || towardEnd > 1) {
    return std::nullopt;
  }
  if (towardEnd == 0) { return endpoints[0]; }
  if (towardEnd == 1) { return endpoints[1]; }
  const Interval t{.Lower = towardEnd, .Upper = towardEnd};
  const Interval from = Subtract({.Lower = 1, .Upper = 1}, t);
  const Vec3 estimate =
      endpoints[0].EstimateM * (1 - towardEnd) + endpoints[1].EstimateM * towardEnd;
  std::array<Interval, 3> rounding{};
  for (size_t axis = 0; axis < 3; ++axis) {
    const double a = endpoints[0].EstimateM[axis];
    const double b = endpoints[1].EstimateM[axis];
    const Interval coordinate =
        Add(Multiply(from, {.Lower = a, .Upper = a}), Multiply(t, {.Lower = b, .Upper = b}));
    rounding[axis] = Subtract(coordinate, {.Lower = estimate[axis], .Upper = estimate[axis]});
  }
  const double deviation = DistanceArithmetic::UpperNorm(rounding);
  const double aRadius = endpoints[0].RadiusM;
  const double bRadius = endpoints[1].RadiusM;
  const Interval inputRadius = Add(Multiply(from, {.Lower = aRadius, .Upper = aRadius}),
                                   Multiply(t, {.Lower = bRadius, .Upper = bRadius}));
  const double radius = Add(inputRadius, {.Lower = deviation, .Upper = deviation}).Upper;
  const PointEnclosure result{.EstimateM = estimate, .RadiusM = radius};
  return ValidPoint(result) ? std::optional<PointEnclosure>(result) : std::nullopt;
}

std::optional<TriangleRegionEnclosure>
EncloseTriangleRegion(const std::array<PointEnclosure, 3> &vertices) noexcept {
  const auto edge = EnclosePointBlend({vertices[0], vertices[1]}, 0.5);
  if (!edge) { return std::nullopt; }
  const auto sample = EnclosePointBlend({*edge, vertices[2]}, 1.0 / 3.0);
  if (!sample) { return std::nullopt; }
  double radius = 0;
  for (const PointEnclosure &vertex : vertices) {
    const auto distance = BoundPointDistance(vertex.EstimateM, sample->EstimateM);
    if (!distance) { return std::nullopt; }
    const double upper = Add({.Lower = *distance, .Upper = *distance},
                             {.Lower = vertex.RadiusM, .Upper = vertex.RadiusM})
                             .Upper;
    radius = std::max(radius, upper);
  }
  if (!std::isfinite(radius)) { return std::nullopt; }
  return TriangleRegionEnclosure{.InteriorSample = *sample, .RadiusM = radius};
}
}
