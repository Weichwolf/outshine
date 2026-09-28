#include "TriangleDistance.h"
#include "DistanceInterval.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <cstddef>

namespace outshine {
namespace {

using DistanceArithmetic::Add;
using DistanceArithmetic::Interval;
using DistanceArithmetic::Multiply;
using DistanceArithmetic::Subtract;

[[nodiscard]] bool Finite(const Vec3 &value) noexcept {
  return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

[[nodiscard]] std::optional<TriangleDistanceBound> Candidate(const Vec3 &point,
                                                             const std::array<Vec3, 3> &cornersM,
                                                             double towardEdge,
                                                             double alongEdge) noexcept {
  const Vec3 &a = cornersM[0];
  const Vec3 &b = cornersM[1];
  const Vec3 &c = cornersM[2];
  if (!std::isfinite(towardEdge) || !std::isfinite(alongEdge)) { return std::nullopt; }
  towardEdge = std::clamp(towardEdge, 0.0, 1.0);
  alongEdge = std::clamp(alongEdge, 0.0, 1.0);
  const Interval t{.Lower = towardEdge, .Upper = towardEdge};
  const Interval s{.Lower = alongEdge, .Upper = alongEdge};
  const Interval one{.Lower = 1, .Upper = 1};
  Interval squared{.Lower = 0, .Upper = 0};
  Vec3 target{};
  for (size_t axis = 0; axis < 3; ++axis) {
    const Interval ai{.Lower = a[axis], .Upper = a[axis]};
    const Interval bi{.Lower = b[axis], .Upper = b[axis]};
    const Interval ci{.Lower = c[axis], .Upper = c[axis]};
    const Interval edge = Add(Multiply(Subtract(one, s), bi), Multiply(s, ci));
    const Interval q = Add(Multiply(Subtract(one, t), ai), Multiply(t, edge));
    const Interval delta = Subtract({.Lower = point[axis], .Upper = point[axis]}, q);
    const double extent = std::max(std::abs(delta.Lower), std::abs(delta.Upper));
    squared = Add(squared,
                  Multiply({.Lower = extent, .Upper = extent}, {.Lower = extent, .Upper = extent}));
    target[axis] =
        (1 - towardEdge) * a[axis] + towardEdge * ((1 - alongEdge) * b[axis] + alongEdge * c[axis]);
  }
  const double upper =
      std::nextafter(std::sqrt(squared.Upper), std::numeric_limits<double>::infinity());
  if (!Finite(target) || !std::isfinite(upper)) { return std::nullopt; }
  return TriangleDistanceBound{.TargetPointEstimateM = target, .UpperDistanceM = upper};
}

[[nodiscard]] Interval DotEnclosure(const Vec3 &direction, const Vec3 &point) noexcept {
  Interval result{.Lower = 0, .Upper = 0};
  for (size_t axis = 0; axis < 3; ++axis) {
    result = Add(result,
                 Multiply({.Lower = direction[axis], .Upper = direction[axis]},
                          {.Lower = point[axis], .Upper = point[axis]}));
  }
  return result;
}

[[nodiscard]] double SupportPlaneLowerDistance(const Vec3 &point,
                                               const std::array<Vec3, 3> &cornersM,
                                               const Vec3 &targetEstimate) noexcept {
  Vec3 direction = point - targetEstimate;
  const double scale =
      std::max({std::abs(direction[0]), std::abs(direction[1]), std::abs(direction[2])});
  if (!(scale > 0) || !std::isfinite(scale)) { return 0; }
  for (size_t axis = 0; axis < 3; ++axis) { direction[axis] /= scale; }
  const auto norm = BoundPointDistance(direction, Vec3{});
  if (!norm || !(*norm > 0)) { return 0; }
  const Interval pointDot = DotEnclosure(direction, point);
  double support = -std::numeric_limits<double>::infinity();
  for (const Vec3 &corner : cornersM) {
    const Interval value = DotEnclosure(direction, corner);
    if (!std::isfinite(value.Upper)) { return 0; }
    support = std::max(support, value.Upper);
  }
  const Interval gap = Subtract(pointDot, {.Lower = support, .Upper = support});
  if (!std::isfinite(gap.Lower)) { return 0; }
  const double lower =
      std::nextafter(std::max(0.0, gap.Lower) / *norm, -std::numeric_limits<double>::infinity());
  return std::max(0.0, lower);
}

[[nodiscard]] double SegmentParameter(const Vec3 &point, const Vec3 &a, const Vec3 &b) noexcept {
  const Vec3 edge = b - a;
  const double lengthSquared = Dot(edge, edge);
  if (!(lengthSquared > 0) || !std::isfinite(lengthSquared)) { return 0; }
  const double projection = Dot(point - a, edge) / lengthSquared;
  return std::isfinite(projection) ? std::clamp(projection, 0.0, 1.0) : 0;
}

}

std::optional<double> BoundPointDistance(const Vec3 &pointM, const Vec3 &targetM) noexcept {
  if (!Finite(pointM) || !Finite(targetM)) { return std::nullopt; }
  const auto bound = Candidate(pointM, {targetM, targetM, targetM}, 0, 0);
  return bound ? std::optional<double>(bound->UpperDistanceM) : std::nullopt;
}

std::optional<TriangleDistanceBound> BoundPointTriangleDistance(const Vec3 &pointM,
                                                                const Vec3 &aM,
                                                                const Vec3 &bM,
                                                                const Vec3 &cM) noexcept {
  if (!Finite(pointM) || !Finite(aM) || !Finite(bM) || !Finite(cM)) { return std::nullopt; }
  std::optional<TriangleDistanceBound> best;
  const auto consider = [&](double towardEdge, double alongEdge) {
    const auto value = Candidate(pointM, {aM, bM, cM}, towardEdge, alongEdge);
    if (value && (!best || value->UpperDistanceM < best->UpperDistanceM)) { best = value; }
  };
  consider(0, 0);
  consider(1, 0);
  consider(1, 1);
  consider(SegmentParameter(pointM, aM, bM), 0);
  consider(SegmentParameter(pointM, aM, cM), 1);
  consider(1, SegmentParameter(pointM, bM, cM));
  const Vec3 ab = bM - aM;
  const Vec3 ac = cM - aM;
  const Vec3 ap = pointM - aM;
  const double aa = Dot(ab, ab);
  const double bb = Dot(ac, ac);
  const double cross = Dot(ab, ac);
  const double determinant = aa * bb - cross * cross;
  if (determinant > 0 && std::isfinite(determinant)) {
    const double u = (Dot(ap, ab) * bb - Dot(ap, ac) * cross) / determinant;
    const double v = (Dot(ap, ac) * aa - Dot(ap, ab) * cross) / determinant;
    const double sum = u + v;
    if (u >= 0 && v >= 0 && sum > 0 && sum <= 1) { consider(sum, v / sum); }
  }
  if (best) {
    best->LowerDistanceM =
        SupportPlaneLowerDistance(pointM, {aM, bM, cM}, best->TargetPointEstimateM);
    if (best->LowerDistanceM > best->UpperDistanceM) { return std::nullopt; }
  }
  return best;
}

}
