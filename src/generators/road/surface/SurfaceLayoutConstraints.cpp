#include "SurfaceLayoutInternal.h"
#include "PlanHierarchy.h"
#include "math/Box.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators::SurfaceLayouts {
Checked<std::vector<PlanarLine>>
NodingLines(const SurfaceLayout &output, double distance, const std::stop_token &stop) {
  if (!std::isfinite(distance) || distance < 0) { return std::unexpected(Error::InvalidInput); }
  auto lines = output.Constraints;
  for (const auto &range : output.Sections) {
    for (uint32_t at = range.First; at < range.First + range.Count; ++at) {
      if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
      auto &line = lines[at];
      const auto first = line.front();
      const auto last = line.back();
      const PlanarPoint direction{last[0] - first[0], last[1] - first[1]};
      const double length = std::hypot(direction[0], direction[1]);
      if (!std::isfinite(length) || length <= 0) { return std::unexpected(Error::InvalidInput); }
      for (size_t axis = 0; axis < first.size(); ++axis) {
        const double extension = direction[axis] / length * distance;
        line.front()[axis] -= extension;
        line.back()[axis] += extension;
      }
    }
  }
  return lines;
}

namespace {
struct Edge {
  std::array<PlanarPoint, 2> Points;
  uint64_t Owners = 0;
};

Box Bounds(std::span<const PlanarPoint> points, double distance) {
  Box box{.Min = {{points[0][0], points[0][1], 0}}, .Max = {{points[0][0], points[0][1], 0}}};
  for (const auto &point : points) {
    for (size_t axis = 0; axis < point.size(); ++axis) {
      box.Min[axis] = std::min(box.Min[axis], point[axis]);
      box.Max[axis] = std::max(box.Max[axis], point[axis]);
    }
  }
  for (size_t axis = 0; axis < points[0].size(); ++axis) {
    box.Min[axis] -= distance;
    box.Max[axis] += distance;
  }
  return box;
}

struct CoverageQuery {
  const PlanHierarchy &Index;
  std::span<const Edge> Edges;
  std::array<PlanarPoint, 2> Segment;
  uint64_t Owner = 0;
  double DistanceM = 0;
  SurfaceLayout &Output;
  SurfaceLayoutBudget Budget;
  const std::stop_token &Stop;
};

Checked<void> Covered(const CoverageQuery &query) {
  const auto first = query.Segment[0];
  const PlanarPoint delta{query.Segment[1][0] - first[0], query.Segment[1][1] - first[1]};
  const double length = std::hypot(delta[0], delta[1]);
  if (length <= query.DistanceM) { return {}; }
  const PlanarPoint direction{delta[0] / length, delta[1] / length};
  std::vector<std::array<double, 2>> intervals;
  const bool complete = query.Index.Query(Bounds(query.Segment, query.DistanceM), [&](uint32_t at) {
    if (query.Stop.stop_requested() ||
        query.Output.Work.ConstraintEdgeTests == query.Budget.ConstraintCandidates) {
      return false;
    }
    ++query.Output.Work.ConstraintEdgeTests;
    const auto &edge = query.Edges[at];
    if ((edge.Owners & query.Owner) == 0) { return true; }
    std::array<double, 2> projected{};
    for (size_t end = 0; end < edge.Points.size(); ++end) {
      const PlanarPoint relative{edge.Points[end][0] - first[0], edge.Points[end][1] - first[1]};
      if (std::abs(relative[0] * direction[1] - relative[1] * direction[0]) > query.DistanceM) {
        return true;
      }
      projected[end] =
          std::clamp(relative[0] * direction[0] + relative[1] * direction[1], 0.0, length);
    }
    if (projected[1] < projected[0]) { std::swap(projected[0], projected[1]); }
    intervals.push_back(projected);
    return true;
  });
  if (!complete) {
    return std::unexpected(query.Stop.stop_requested() ? Error::Canceled : Error::CapacityExceeded);
  }
  std::ranges::sort(intervals);
  double cursor = 0;
  for (const auto &interval : intervals) {
    if (interval[0] > cursor + query.DistanceM) { return std::unexpected(Error::ConstraintLost); }
    cursor = std::max(cursor, interval[1]);
  }
  if (cursor + query.DistanceM < length) { return std::unexpected(Error::ConstraintLost); }
  return {};
}
}

Checked<void> ValidateSections(SurfaceLayout &output,
                               double distance,
                               SurfaceLayoutBudget budget,
                               const std::stop_token &stop) {
  if (output.Work.SectionParts == 0) { return {}; }
  std::vector<Edge> edges;
  std::vector<Box> boxes;
  const auto &mesh = output.Arrangement.Mesh;
  edges.reserve(3 * mesh.Faces.size());
  boxes.reserve(3 * mesh.Faces.size());
  for (size_t face = 0; face < mesh.Faces.size(); ++face) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    for (size_t side = 0; side < mesh.Faces[face].size(); ++side) {
      const auto &triangle = mesh.Faces[face];
      const std::array<PlanarPoint, 2> points{mesh.Points[triangle[side]],
                                              mesh.Points[triangle[(side + 1) % triangle.size()]]};
      edges.push_back({.Points = points, .Owners = output.Arrangement.FaceDomains[face]});
      boxes.push_back(Bounds(points, 0));
    }
  }
  const auto index = PlanHierarchy::Build(boxes);
  if (!index) { return std::unexpected(Error::CapacityExceeded); }
  for (size_t source = 0; source < output.Sections.size(); ++source) {
    const auto range = output.Sections[source];
    for (uint32_t line = range.First; line < range.First + range.Count; ++line) {
      const auto &points = output.Constraints[line];
      for (size_t at = 1; at < points.size(); ++at) {
        ++output.Work.ConstraintQueries;
        const auto covered = Covered({.Index = *index,
                                      .Edges = edges,
                                      .Segment = {points[at - 1], points[at]},
                                      .Owner = uint64_t{1} << source,
                                      .DistanceM = distance,
                                      .Output = output,
                                      .Budget = budget,
                                      .Stop = stop});
        if (!covered) { return covered; }
      }
    }
  }
  return {};
}
}
