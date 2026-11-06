#include "RibbonEnvelope.h"
#include "DistanceInterval.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace outshine {
namespace {
using namespace DistanceArithmetic;
using Point = std::array<Interval, 3>;

Interval Exact(double v) noexcept {
  return {.Lower = v, .Upper = v};
}

bool Finite(Interval v) noexcept {
  return std::isfinite(v.Lower) && std::isfinite(v.Upper) && v.Lower <= v.Upper;
}

Point Exact(const Vec3f &p, RibbonRoof roof) noexcept {
  return {Exact(p[0]), Exact(roof == RibbonRoof::Lower ? -p[1] : p[1]), Exact(p[2])};
}

Point Difference(const Point &a, const Point &b) noexcept {
  return {Subtract(a[0], b[0]), Subtract(a[1], b[1]), Subtract(a[2], b[2])};
}

Interval CrossXZ(const Point &a, const Point &b) noexcept {
  return Subtract(Multiply(a[0], b[2]), Multiply(a[2], b[0]));
}

std::optional<Interval> Divide(Interval a, Interval b) noexcept {
  if (!Finite(a) || !Finite(b) || (b.Lower <= 0 && b.Upper >= 0)) { return std::nullopt; }
  const auto result = Multiply(a, Enclose(1 / b.Upper, 1 / b.Lower));
  return Finite(result) ? std::optional(result) : std::nullopt;
}

std::optional<bool> Convex(const std::array<Point, 4> &points) noexcept {
  bool positive = true;
  bool negative = true;
  for (size_t at = 0; at < 4; ++at) {
    const auto a = Difference(points[(at + 1) % 4], points[at]);
    const auto b = Difference(points[(at + 2) % 4], points[(at + 1) % 4]);
    const auto turn = CrossXZ(a, b);
    if (!Finite(turn)) { return std::nullopt; }
    positive &= turn.Lower > 0;
    negative &= turn.Upper < 0;
  }
  if (!positive && !negative) { return std::nullopt; }
  return positive;
}

std::optional<double> RingResidual(std::span<const Vec3f, 4> ring) noexcept {
  const Vec3 a{{ring[0][0], ring[0][1], ring[0][2]}};
  const Vec3 b{{ring[3][0], ring[3][1], ring[3][2]}};
  const Vec3 direction = b - a;
  const double length2 = Dot(direction, direction);
  if (!(length2 > 0) || !std::isfinite(length2)) { return std::nullopt; }
  double previous = -1;
  double error = 0;
  const auto start = Exact(ring[0], RibbonRoof::Upper);
  const auto span = Difference(Exact(ring[3], RibbonRoof::Upper), start);
  for (const auto &p : ring) {
    const Vec3 source{{p[0], p[1], p[2]}};
    const double t = std::clamp(Dot(source - a, direction) / length2, 0.0, 1.0);
    if (!std::isfinite(t) || !(t > previous)) { return std::nullopt; }
    previous = t;
    Point residual;
    for (size_t axis = 0; axis < 3; ++axis) {
      residual[axis] =
          Subtract(Exact(source[axis]), Add(start[axis], Multiply(Exact(t), span[axis])));
    }
    error = std::max(error, UpperNorm(residual));
  }
  return std::isfinite(error) ? std::optional(error) : std::nullopt;
}

struct RoofSplit {
  bool Across = false;
  double GapM = 0;
};

std::optional<RoofSplit> Split(const std::array<Point, 4> &p) noexcept {
  const auto ac = Difference(p[2], p[0]);
  const auto bd = Difference(p[3], p[1]);
  const auto ab = Difference(p[1], p[0]);
  const auto denominator = CrossXZ(ac, bd);
  const auto t = Divide(CrossXZ(ab, bd), denominator);
  const auto u = Divide(CrossXZ(ab, ac), denominator);
  if (!t || !u || t->Lower <= 0 || t->Upper >= 1 || u->Lower <= 0 || u->Upper >= 1) {
    return std::nullopt;
  }
  const auto a = Add(p[0][1], Multiply(*t, ac[1]));
  const auto b = Add(p[1][1], Multiply(*u, bd[1]));
  const auto gap = Subtract(a, b);
  if (!Finite(gap)) { return std::nullopt; }
  if (p[0][1].Lower == p[1][1].Lower && p[0][1].Lower == p[2][1].Lower &&
      p[0][1].Lower == p[3][1].Lower) {
    return RoofSplit{};
  }
  if (gap.Lower <= 0 && gap.Upper >= 0) { return std::nullopt; }
  return RoofSplit{.Across = gap.Lower > 0,
                   .GapM = std::max(std::abs(gap.Lower), std::abs(gap.Upper))};
}
}

std::optional<RibbonEnvelope> BoundRibbonSpan(std::span<const Vec3f, 4> first,
                                              std::span<const Vec3f, 4> second,
                                              RibbonRoof roof) noexcept {
  for (const auto ring : {first, second}) {
    for (const auto &p : ring) {
      if (!std::ranges::all_of(p, [](float v) { return std::isfinite(v); })) {
        return std::nullopt;
      }
    }
  }
  const std::array outer{
      Exact(first[0], roof), Exact(first[3], roof), Exact(second[3], roof), Exact(second[0], roof)};
  const auto convex = Convex(outer);
  const auto split = Split(outer);
  const auto a = RingResidual(first);
  const auto b = RingResidual(second);
  if (!convex || !split || !a || !b) { return std::nullopt; }
  const double residual = std::max(*a, *b);
  const double bound = Add(Exact(split->GapM), Multiply(Exact(2), Exact(residual))).Upper;
  if (!std::isfinite(bound)) { return std::nullopt; }
  RibbonEnvelope result{.Triangles = split->Across ? std::array<uint8_t, 6>{0, 4, 7, 0, 7, 3}
                                                   : std::array<uint8_t, 6>{0, 4, 3, 3, 4, 7},
                        .PositionBoundM = bound,
                        .RingResidualM = residual};
  if (*convex == (roof == RibbonRoof::Lower)) {
    for (size_t at = 0; at < result.Triangles.size(); at += 3) {
      std::swap(result.Triangles[at + 1], result.Triangles[at + 2]);
    }
  }
  return result;
}
}
