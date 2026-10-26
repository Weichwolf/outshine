#include "TransportCoreClipping.h"
#include "Geodesy.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>

namespace outshine::Generators {
namespace {
struct Segment {
  double Longitude = 0;
  double Latitude = 0;
  double EndLongitude = 0;
  double EndLatitude = 0;
};

struct Cut {
  double Fraction = 0;
  uint8_t Sides = 0;
};

struct Interval {
  Cut Begin{};
  Cut End{.Fraction = 1};
  uint8_t ParallelSides = 0;
};

double CanonicalLongitude(double longitude) {
  const double wrapped = Wrap180(longitude);
  if (wrapped == kDegPerHalfTurn) { return -kDegPerHalfTurn; }
  return wrapped == 0 ? 0 : wrapped;
}

bool ClipBoundary(double first,
                  double last,
                  double bound,
                  TransportCoreSide side,
                  Interval &interval,
                  TransportCoreWork &work) {
  const bool lower = side == TransportCoreSide::West || side == TransportCoreSide::South;
  const bool firstInside = lower ? first >= bound : first <= bound;
  const bool lastInside = lower ? last >= bound : last <= bound;
  const auto bit = static_cast<uint8_t>(1u << static_cast<uint32_t>(side));
  if (firstInside && lastInside) {
    if (first == bound && last == bound) {
      interval.ParallelSides |= bit;
    } else {
      if (first == bound && interval.Begin.Fraction == 0) { interval.Begin.Sides |= bit; }
      if (last == bound && interval.End.Fraction == 1) { interval.End.Sides |= bit; }
    }
    return true;
  }
  if (!firstInside && !lastInside) { return false; }
  ++work.ClipDivisions;
  const double fraction = (bound - first) / (last - first);
  if (lastInside) {
    if (fraction > interval.Begin.Fraction) {
      interval.Begin.Fraction = fraction;
      interval.Begin.Sides = bit;
    } else if (fraction == interval.Begin.Fraction) {
      interval.Begin.Sides |= bit;
    }
  } else if (fraction < interval.End.Fraction) {
    interval.End.Fraction = fraction;
    interval.End.Sides = bit;
  } else if (fraction == interval.End.Fraction) {
    interval.End.Sides |= bit;
  }
  return interval.Begin.Fraction < interval.End.Fraction;
}

TransportCoreEnd
EndAt(const TransportCoreWindow &window, const Segment &source, Cut cut, double shift) {
  TransportCoreEnd end{
      .Fraction = cut.Fraction,
      .Geographic = {.LongitudeDeg = CanonicalLongitude(
                         std::lerp(source.Longitude, source.EndLongitude, cut.Fraction)),
                     .LatitudeDeg = std::lerp(source.Latitude, source.EndLatitude, cut.Fraction)}};
  const auto &bounds = window.Bounds;
  const std::array<double, 4> planes{
      bounds.WestDeg + shift, bounds.EastDeg + shift, bounds.SouthDeg, bounds.NorthDeg};
  size_t boundary = 0;
  for (uint32_t side = 0; side < planes.size(); ++side) {
    if ((cut.Sides & (1u << side)) == 0) { continue; }
    end.Boundaries[boundary++] = window.Boundaries[side];
    if (side < 2) {
      end.Geographic.LongitudeDeg = CanonicalLongitude(planes[side]);
    } else {
      end.Geographic.LatitudeDeg = planes[side];
    }
  }
  return end;
}

bool ClipWindow(const TransportCoreWindow &window,
                uint32_t edge,
                const Segment &source,
                double shift,
                TransportCoreSegment &result,
                TransportCoreWork &work) {
  ++work.WindowTests;
  const auto &bounds = window.Bounds;
  const double west = bounds.WestDeg + shift;
  const double east = bounds.EastDeg + shift;
  if ((source.Longitude < west && source.EndLongitude < west) ||
      (source.Longitude > east && source.EndLongitude > east) ||
      (source.Latitude < bounds.SouthDeg && source.EndLatitude < bounds.SouthDeg) ||
      (source.Latitude > bounds.NorthDeg && source.EndLatitude > bounds.NorthDeg)) {
    ++work.BoundingRejections;
    return false;
  }
  if ((source.Longitude == source.EndLongitude && source.Longitude == east) ||
      (source.Latitude == source.EndLatitude && source.Latitude == bounds.NorthDeg &&
       bounds.NorthDeg != kDegPerHalfTurn * 0.5)) {
    return false;
  }
  Interval interval;
  if (!ClipBoundary(
          source.Longitude, source.EndLongitude, west, TransportCoreSide::West, interval, work) ||
      !ClipBoundary(
          source.Longitude, source.EndLongitude, east, TransportCoreSide::East, interval, work) ||
      !ClipBoundary(source.Latitude,
                    source.EndLatitude,
                    bounds.SouthDeg,
                    TransportCoreSide::South,
                    interval,
                    work) ||
      !ClipBoundary(source.Latitude,
                    source.EndLatitude,
                    bounds.NorthDeg,
                    TransportCoreSide::North,
                    interval,
                    work) ||
      !(interval.Begin.Fraction < interval.End.Fraction)) {
    return false;
  }
  interval.Begin.Sides |= interval.ParallelSides;
  interval.End.Sides |= interval.ParallelSides;
  result.SourceEdge = edge;
  result.Ends = {EndAt(window, source, interval.Begin, shift),
                 EndAt(window, source, interval.End, shift)};
  return true;
}
}

std::expected<TransportCoreWindow, TransportCoreError> TransportWindowOf(GeoCellId core) noexcept {
  const auto bounds = core.Bounds();
  if (!bounds) { return std::unexpected(TransportCoreError::InvalidCore); }
  TransportCoreWindow window{.Bounds = *bounds};
  constexpr std::array sides{TransportCoreSide::West,
                             TransportCoreSide::East,
                             TransportCoreSide::South,
                             TransportCoreSide::North};
  for (size_t at = 0; at < sides.size(); ++at) {
    const auto boundary = TransportBoundaryOf(core, sides[at]);
    if (!boundary) { return std::unexpected(TransportCoreError::InvalidCore); }
    window.Boundaries[at] = *boundary;
  }
  return window;
}

std::expected<ClippedTransportSegments, TransportCoreError>
ClipTransportCore(const TransportCoreWindow &window,
                  uint32_t edge,
                  const World::TransportNode &from,
                  const World::TransportNode &to,
                  TransportCoreWork &work) noexcept {
  const double start = CanonicalLongitude(from.LongitudeDeg);
  const double delta = CanonicalLongitude(CanonicalLongitude(to.LongitudeDeg) - start);
  if (std::abs(delta) == kDegPerHalfTurn) {
    return std::unexpected(TransportCoreError::AmbiguousSegment);
  }
  const Segment source{.Longitude = start,
                       .Latitude = from.LatitudeDeg,
                       .EndLongitude = start + delta,
                       .EndLatitude = to.LatitudeDeg};
  ClippedTransportSegments result;
  for (int period = -1; period <= 1; ++period) {
    TransportCoreSegment clipped;
    if (ClipWindow(window, edge, source, period * kDegPerTurn, clipped, work)) {
      if (result.Count == result.Segments.size()) {
        return std::unexpected(TransportCoreError::InvalidSource);
      }
      result.Segments[result.Count++] = clipped;
    }
  }
  if (result.Count == 2 &&
      result.Segments[1].Ends[0].Fraction < result.Segments[0].Ends[0].Fraction) {
    std::swap(result.Segments[0], result.Segments[1]);
  }
  return result;
}
}
