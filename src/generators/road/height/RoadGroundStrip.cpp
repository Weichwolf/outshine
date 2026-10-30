#include "RoadGroundStrip.h"
#include "RoadGroundStripInternal.h"
#include "RoadPortFrame.h"
#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <numeric>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr size_t kAxisPointsMost = 256;
constexpr size_t kControlsMost = 256;
constexpr double kWidthMostM = 30;
constexpr double kGradientMost = 10;

EastNorth Interpolate(std::span<const EastNorth> values,
                      const RoadGround::Axis &axis,
                      size_t segment,
                      double at) {
  const double fraction =
      (at - axis.Stations[segment]) / (axis.Stations[segment + 1] - axis.Stations[segment]);
  return {.EastM = std::lerp(values[segment].EastM, values[segment + 1].EastM, fraction),
          .NorthM = std::lerp(values[segment].NorthM, values[segment + 1].NorthM, fraction)};
}

size_t SegmentAt(std::span<const double> stations, double at) {
  const auto end = std::ranges::upper_bound(stations, at);
  const size_t index = static_cast<size_t>(end - stations.begin());
  return std::min(index == 0 ? 0 : index - 1, stations.size() - 2);
}

std::expected<void, RoadGroundError> Queries(RoadGroundStrip &strip,
                                             const RoadGround::Axis &axis,
                                             std::span<const double> stations,
                                             double widthM,
                                             const std::stop_token &stop) {
  const auto frame = TangentFrame::At(strip.Anchor);
  strip.TerrainQueries.reserve(2 * stations.size());
  for (const double at : stations) {
    if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
    const size_t segment = SegmentAt(axis.Stations, at);
    const auto centre = Interpolate(axis.Points, axis, segment, at);
    const auto miter = Interpolate(axis.Miters, axis, segment, at);
    for (const double side : std::array{-1., 1.}) {
      const auto query =
          RoadPortGroundPosition(frame,
                                 {.EastM = centre.EastM + side * widthM / 2 * miter.EastM,
                                  .NorthM = centre.NorthM + side * widthM / 2 * miter.NorthM});
      if (!query) { return std::unexpected(RoadGroundError::InvalidGeometry); }
      strip.TerrainQueries.push_back(*query);
    }
  }
  return {};
}

void Faces(RoadGroundStrip &strip,
           std::span<const double> stations,
           const RoadGround::Axis &axis,
           std::span<const double> boundaries,
           double maximumGradient) {
  const size_t intervals = stations.size() - 1;
  strip.Triangles.reserve(2 * intervals);
  strip.FacePatches.reserve(2 * intervals);
  strip.TriangleAxisSegments.reserve(2 * intervals);
  strip.GradeFrames.reserve(2 * intervals);
  for (size_t group = 0; group < 2; ++group) {
    for (size_t at = 0; at < intervals; ++at) {
      const auto first = static_cast<uint32_t>(2 * at);
      strip.Triangles.push_back(group == 0 ? std::array{first, first + 2, first + 1}
                                           : std::array{first + 2, first + 3, first + 1});
      const double midpoint = std::midpoint(stations[at], stations[at + 1]);
      const auto owner = std::ranges::upper_bound(boundaries, midpoint) - boundaries.begin() - 1;
      strip.FacePatches.push_back(static_cast<uint32_t>(owner));
      const size_t segment = SegmentAt(axis.Stations, midpoint);
      strip.TriangleAxisSegments.push_back(static_cast<uint32_t>(segment));
      const auto tangent = axis.Tangents[segment];
      strip.GradeFrames.push_back({tangent.EastM / maximumGradient,
                                   tangent.NorthM / maximumGradient,
                                   -tangent.NorthM / kCrossfall,
                                   tangent.EastM / kCrossfall});
    }
  }
}
}

std::expected<RoadGroundStrip, RoadGroundError>
PrepareRoadGroundStrip(std::span<const LongitudeLatitude> geographic,
                       double widthM,
                       double maximumGradient,
                       RoadGroundBudget budget,
                       const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
  if (geographic.size() < 2 || geographic.size() > kAxisPointsMost || !std::isfinite(widthM) ||
      widthM <= 0 || widthM > kWidthMostM || !std::isfinite(maximumGradient) ||
      maximumGradient <= 0 || maximumGradient > kGradientMost) {
    return std::unexpected(RoadGroundError::UnsupportedRecipe);
  }
  if (geographic.size() > budget.AxisPoints ||
      geographic.size() > budget.ContainerBytes / (sizeof(EastNorth) * 3 + sizeof(double))) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  if (!RoadGround::Valid(geographic.front())) {
    return std::unexpected(RoadGroundError::InvalidAxis);
  }
  const auto axis = RoadGround::Frame(geographic, widthM, stop);
  if (!axis) { return std::unexpected(axis.error()); }
  const auto boundaries =
      RoadGround::Bands(axis->Stations, std::min(budget.Controls, kControlsMost));
  if (!boundaries) { return std::unexpected(boundaries.error()); }
  std::vector<double> stations = *boundaries;
  stations.insert(stations.end(), axis->Stations.begin(), axis->Stations.end());
  std::ranges::sort(stations);
  const auto repeated = std::ranges::unique(stations);
  stations.erase(repeated.begin(), stations.end());
  if (stations.size() > budget.TerrainSamples / 2) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  constexpr size_t kStationBytes =
      2 * sizeof(LongitudeLatitude) +
      2 * (sizeof(std::array<uint32_t, 3>) + 2 * sizeof(uint32_t) + sizeof(std::array<double, 4>));
  if (stations.size() > budget.ContainerBytes / kStationBytes) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  RoadGroundStrip strip{.Anchor = geographic.front(),
                        .TerrainQueries = {},
                        .Triangles = {},
                        .TriangleAxisSegments = {},
                        .FacePatches = {},
                        .GradeFrames = {},
                        .Boundaries = {},
                        .Work = {}};
  strip.Boundaries.reserve(boundaries->size());
  for (const double boundary : *boundaries) {
    strip.Boundaries.push_back(
        static_cast<uint32_t>(std::ranges::lower_bound(stations, boundary) - stations.begin()));
  }
  const auto queries = Queries(strip, *axis, stations, widthM, stop);
  if (!queries) { return std::unexpected(queries.error()); }
  Faces(strip, stations, *axis, *boundaries, maximumGradient);
  strip.Work = {.AxisPoints = geographic.size(),
                .Controls = boundaries->size() / 2,
                .TerrainSamples = strip.TerrainQueries.size(),
                .Faces = strip.Triangles.size()};
  strip.Work.ContainerCapacityBytes =
      strip.TerrainQueries.capacity() * sizeof(LongitudeLatitude) +
      strip.Triangles.capacity() * sizeof(std::array<uint32_t, 3>) +
      (strip.FacePatches.capacity() + strip.TriangleAxisSegments.capacity()) * sizeof(uint32_t) +
      strip.GradeFrames.capacity() * sizeof(std::array<double, 4>) +
      strip.Boundaries.capacity() * sizeof(uint32_t);
  if (strip.Work.ContainerCapacityBytes > budget.ContainerBytes) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  return strip;
}
}
