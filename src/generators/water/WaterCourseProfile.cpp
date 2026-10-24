#include "WaterCourseProfile.h"
#include "Box.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Error = WaterProfileError;

double DistanceSquared(const Box &box, EastNorth point) {
  const double x = std::max({box.Min[0] - point.EastM, 0.0, point.EastM - box.Max[0]});
  const double y = std::max({box.Min[1] - point.NorthM, 0.0, point.NorthM - box.Max[1]});
  return x * x + y * y;
}

struct Projection {
  double Along, DistanceSquared;
};

Projection Project(const WaterCourseSegment &segment, double lengthSquared, EastNorth point) {
  const double x = segment.Points[1].EastM - segment.Points[0].EastM;
  const double y = segment.Points[1].NorthM - segment.Points[0].NorthM;
  const double qx = point.EastM - segment.Points[0].EastM;
  const double qy = point.NorthM - segment.Points[0].NorthM;
  const double along = (qx * x + qy * y) / lengthSquared;
  const double clamped = std::clamp(along, 0.0, 1.0);
  const double dx = qx - clamped * x;
  const double dy = qy - clamped * y;
  return {.Along = along, .DistanceSquared = dx * dx + dy * dy};
}

template <class Emit>
bool Visit(const PlanHierarchy &hierarchy,
           EastNorth point,
           double &distanceSquared,
           Emit &&emit,
           uint64_t &visited) {
  struct Pending {
    uint32_t Index;
    double DistanceSquared;
  };

  std::array<Pending, 32> pending{};
  size_t count = 1;
  pending[0] = {.Index = 0,
                .DistanceSquared = DistanceSquared(hierarchy.Nodes().front().Bounds, point)};
  while (count != 0) {
    const auto next = pending[--count];
    if (next.DistanceSquared > distanceSquared) { continue; }
    const auto &node = hierarchy.Nodes()[next.Index];
    ++visited;
    if (node.Count == 1) {
      if (!emit(hierarchy.SourceOrder()[node.First])) { return false; }
      continue;
    }
    Pending first{.Index = node.Left,
                  .DistanceSquared = DistanceSquared(hierarchy.Nodes()[node.Left].Bounds, point)};
    Pending second{.Index = node.Right,
                   .DistanceSquared = DistanceSquared(hierarchy.Nodes()[node.Right].Bounds, point)};
    if (first.DistanceSquared > second.DistanceSquared) { std::swap(first, second); }
    if (second.DistanceSquared <= distanceSquared) { pending[count++] = second; }
    if (first.DistanceSquared <= distanceSquared) { pending[count++] = first; }
  }
  return true;
}
}

std::expected<WaterCourseProfile, WaterProfileError>
WaterCourseProfile::Build(std::span<const WaterCourseSegment> segments, uint32_t segmentsMost) {
  if (segments.empty()) { return std::unexpected(Error::InvalidInput); }
  if (segments.size() > segmentsMost || segmentsMost > MaximumSegments) {
    return std::unexpected(Error::CapacityExceeded);
  }
  WaterCourseProfile profile;
  std::vector<Box> bounds;
  bounds.reserve(segments.size());
  profile.LengthsSquared_.reserve(segments.size());
  for (const auto &segment : segments) {
    const auto a = segment.Points[0];
    const auto b = segment.Points[1];
    const double x = b.EastM - a.EastM;
    const double y = b.NorthM - a.NorthM;
    const double lengthSquared = x * x + y * y;
    if (!std::isfinite(a.EastM) || !std::isfinite(a.NorthM) || !std::isfinite(b.EastM) ||
        !std::isfinite(b.NorthM) || !std::isfinite(segment.ModelHeightsM[0]) ||
        !std::isfinite(segment.ModelHeightsM[1]) || !std::isfinite(lengthSquared) ||
        lengthSquared == 0) {
      return std::unexpected(Error::InvalidInput);
    }
    bounds.push_back({.Min = {std::min(a.EastM, b.EastM), std::min(a.NorthM, b.NorthM), 0},
                      .Max = {std::max(a.EastM, b.EastM), std::max(a.NorthM, b.NorthM), 0}});
    profile.LengthsSquared_.push_back(lengthSquared);
  }
  auto hierarchy = PlanHierarchy::Build(bounds);
  if (!hierarchy) { return std::unexpected(Error::InvalidInput); }
  profile.Hierarchy_ = std::move(*hierarchy);
  profile.Segments_.assign(segments.begin(), segments.end());
  return profile;
}

std::expected<WaterProfileSample, WaterProfileError>
WaterCourseProfile::Sample(EastNorth point, double blendM, uint32_t referencesMost) const {
  if (!std::isfinite(point.EastM) || !std::isfinite(point.NorthM) || !std::isfinite(blendM) ||
      blendM <= 0 || referencesMost == 0 || referencesMost > MaximumReferences ||
      Segments_.empty()) {
    return std::unexpected(Error::InvalidInput);
  }
  WaterProfileSample sample;
  double nearest = std::numeric_limits<double>::infinity();
  const bool valid = Visit(
      Hierarchy_,
      point,
      nearest,
      [&](uint32_t index) {
        ++sample.SegmentTests;
        const auto projection = Project(Segments_[index], LengthsSquared_[index], point);
        if (!std::isfinite(projection.Along) || !std::isfinite(projection.DistanceSquared)) {
          return false;
        }
        if (projection.DistanceSquared < nearest ||
            (projection.DistanceSquared == nearest && index < sample.NearestSegment)) {
          nearest = projection.DistanceSquared;
          sample.NearestSegment = index;
        }
        return true;
      },
      sample.NodesVisited);
  const double radius = blendM + 2 * std::sqrt(nearest);
  double radiusSquared = radius * radius;
  if (!valid || !std::isfinite(radiusSquared)) { return std::unexpected(Error::InvalidInput); }
  std::array<uint32_t, MaximumReferences> references{};
  const bool fits = Visit(
      Hierarchy_,
      point,
      radiusSquared,
      [&](uint32_t index) {
        ++sample.SegmentTests;
        const auto projection = Project(Segments_[index], LengthsSquared_[index], point);
        if (projection.DistanceSquared <= radiusSquared) {
          if (sample.References == referencesMost) { return false; }
          references[sample.References++] = index;
        }
        return true;
      },
      sample.NodesVisited);
  if (!fits) { return std::unexpected(Error::CapacityExceeded); }
  const auto selected = std::span(references).first(sample.References);
  std::ranges::sort(selected);
  double total = 0;
  double weighted = 0;
  for (const auto index : selected) {
    ++sample.SegmentTests;
    const auto &segment = Segments_[index];
    const auto projection = Project(segment, LengthsSquared_[index], point);
    const double ratio = std::clamp(std::sqrt(projection.DistanceSquared) / radius, 0.0, 1.0);
    const double rest = 1 - ratio;
    const double weight = rest * rest * rest * (1 + 3 * ratio + 6 * ratio * ratio);
    const double value = segment.ModelHeightsM[0] * (1 - projection.Along) +
                         segment.ModelHeightsM[1] * projection.Along;
    total += weight;
    weighted += weight * value;
  }
  sample.ModelHeightM = weighted / total;
  if (!std::isfinite(sample.ModelHeightM) || total <= 0) {
    return std::unexpected(Error::InvalidInput);
  }
  return sample;
}
}
