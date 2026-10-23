#include "PlanarMeshInternal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::PlanarMeshDetail {
namespace {
std::expected<PlanHierarchy, PlanarMeshError> EdgeIndex(std::span<const PlanarPoint> points,
                                                        std::span<const Edge> edges) {
  std::vector<Box> boxes;
  boxes.reserve(edges.size());
  for (const auto edge : edges) {
    const auto a = points[edge[0]];
    const auto b = points[edge[1]];
    boxes.push_back({.Min = {{std::min(a[0], b[0]), std::min(a[1], b[1]), 0}},
                     .Max = {{std::max(a[0], b[0]), std::max(a[1], b[1]), 0}}});
  }
  auto index = PlanHierarchy::Build(boxes);
  if (!index) { return std::unexpected(PlanarMeshError::InvalidInput); }
  return std::move(*index);
}

std::expected<double, PlanarMeshError>
Station(PlanarPoint point, PlanarPoint a, PlanarPoint b, double tolerance) {
  const double x = b[0] - a[0];
  const double y = b[1] - a[1];
  const double px = point[0] - a[0];
  const double py = point[1] - a[1];
  const double square = x * x + y * y;
  const double along = (px * x + py * y) / square;
  const double cross = x * py - y * px;
  if (!(square > 0) || !std::isfinite(square) || !std::isfinite(along) || !std::isfinite(cross)) {
    return std::unexpected(PlanarMeshError::InvalidInput);
  }
  return along > 0 && along < 1 && std::abs(cross) <= tolerance * std::sqrt(square) ? along : -1.0;
}
}

std::expected<std::vector<Split>, PlanarMeshError> FindSplits(PlanarMesh &mesh,
                                                              std::span<const Edge> edges,
                                                              PlanarMeshBudget budget,
                                                              const std::stop_token &stop) {
  auto index = EdgeIndex(mesh.Points, edges);
  if (!index) { return std::unexpected(index.error()); }
  std::vector<Split> splits;
  for (uint32_t point = 0; point < mesh.Points.size(); ++point) {
    if (stop.stop_requested()) { return std::unexpected(PlanarMeshError::Canceled); }
    std::optional<PlanarMeshError> error;
    const auto emit = [&](uint32_t edgeIndex) {
      if (mesh.Work.WeldCandidates + mesh.Work.EdgeCandidates == budget.Candidates) {
        error = PlanarMeshError::CapacityExceeded;
        return false;
      }
      ++mesh.Work.EdgeCandidates;
      const auto edge = edges[edgeIndex];
      if (point == edge[0] || point == edge[1]) { return true; }
      const auto along = Station(
          mesh.Points[point], mesh.Points[edge[0]], mesh.Points[edge[1]], mesh.Work.EdgeTolerance);
      if (!along) {
        error = along.error();
        return false;
      }
      if (*along > 0) {
        splits.push_back({.EdgeIndex = edgeIndex, .Point = point, .Station = *along});
      }
      return true;
    };
    if (!index->Query(PointBox(mesh.Points[point], mesh.Work.EdgeTolerance), emit)) {
      return std::unexpected(error.value_or(PlanarMeshError::InvalidInput));
    }
  }
  std::ranges::sort(splits, [](const Split &a, const Split &b) {
    return std::tie(a.EdgeIndex, a.Station, a.Point) < std::tie(b.EdgeIndex, b.Station, b.Point);
  });
  return splits;
}
}
