#include "WaterBoundaryIntervals.h"

#include "TileGeodesy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
using Interval = WaterBoundaryInterval;
constexpr double kLongitudePeriodDeg = 360.0;

std::array<double, 2> ProjectedPoint(std::span<const double> points, size_t at) {
  const double longitude = points[at * 2 + 1];
  const auto projected =
      Ground::ToTileFracClamped({.LongitudeDeg = longitude, .LatitudeDeg = points[at * 2]}, 0);
  return {(longitude + kLongitudePeriodDeg / 2.0) / kLongitudePeriodDeg, projected.Y};
}

std::vector<Interval> UnionIntervals(std::vector<Interval> intervals) {
  std::ranges::sort(intervals, {}, &Interval::From);
  size_t kept = 0;
  for (const Interval &one : intervals) {
    if (one.To - one.From <= kWaterBoundaryTolerance) { continue; }
    if (kept > 0 && one.From <= intervals[kept - 1].To + kWaterBoundaryTolerance) {
      intervals[kept - 1].To = std::max(intervals[kept - 1].To, one.To);
    } else {
      intervals[kept++] = one;
    }
  }
  intervals.resize(kept);
  return intervals;
}

std::vector<Interval> RingIntervals(std::span<const double> points,
                                    const WaterField::SurfaceRing &ring,
                                    WaterBoundaryCut cut) {
  const size_t across = cut.Axis == WaterBoundaryAxis::X ? 0 : 1;
  const size_t along = 1 - across;
  std::vector<double> crossings;
  std::vector<Interval> intervals;
  auto previous =
      ProjectedPoint(points, static_cast<size_t>(ring.FirstPoint) + ring.PointCount - 1);
  for (size_t step = 0; step < ring.PointCount; ++step) {
    const auto current = ProjectedPoint(points, static_cast<size_t>(ring.FirstPoint) + step);
    if (std::abs(previous[across] - cut.Coordinate) <= kWaterBoundaryTolerance &&
        std::abs(current[across] - cut.Coordinate) <= kWaterBoundaryTolerance) {
      intervals.push_back({.From = std::min(previous[along], current[along]),
                           .To = std::max(previous[along], current[along])});
    } else if ((previous[across] <= cut.Coordinate) != (current[across] <= cut.Coordinate)) {
      const double fraction =
          (cut.Coordinate - previous[across]) / (current[across] - previous[across]);
      crossings.push_back(std::lerp(previous[along], current[along], fraction));
    }
    previous = current;
  }
  if (crossings.size() % 2 != 0) { return {}; }
  std::ranges::sort(crossings);
  for (size_t at = 0; at < crossings.size(); at += 2) {
    intervals.push_back({.From = crossings[at], .To = crossings[at + 1]});
  }
  for (Interval &one : intervals) {
    one.From = std::max(one.From, cut.From);
    one.To = std::min(one.To, cut.To);
  }
  return UnionIntervals(std::move(intervals));
}

}

std::vector<WaterBoundaryInterval> WaterBoundaryIntervals(
    const OsmField &field, std::span<const WaterField::SurfaceRing> rings, WaterBoundaryCut cut) {
  if (rings.empty()) { return {}; }
  const auto exterior = RingIntervals(field.Points(), rings.front(), cut);
  std::vector<Interval> holes;
  for (const auto &ring : rings.subspan(1)) {
    auto intervals = RingIntervals(field.Points(), ring, cut);
    holes.insert(holes.end(), intervals.begin(), intervals.end());
  }
  holes = UnionIntervals(std::move(holes));
  std::vector<Interval> result;
  size_t firstHole = 0;
  for (const Interval &one : exterior) {
    double from = one.From;
    while (firstHole < holes.size() && holes[firstHole].To <= from) { ++firstHole; }
    for (size_t at = firstHole; at < holes.size() && holes[at].From < one.To; ++at) {
      const double to = std::min(one.To, holes[at].From);
      if (to - from > kWaterBoundaryTolerance) { result.push_back({.From = from, .To = to}); }
      from = std::max(from, holes[at].To);
    }
    if (one.To - from > kWaterBoundaryTolerance) { result.push_back({.From = from, .To = one.To}); }
  }
  return result;
}

bool WaterBoundariesOverlap(std::span<const WaterBoundaryInterval> first,
                            std::span<const WaterBoundaryInterval> second) {
  size_t a = 0;
  size_t b = 0;
  while (a < first.size() && b < second.size()) {
    if (std::min(first[a].To, second[b].To) - std::max(first[a].From, second[b].From) >
        kWaterBoundaryTolerance) {
      return true;
    }
    if (first[a].To < second[b].To) {
      ++a;
    } else {
      ++b;
    }
  }
  return false;
}
}
