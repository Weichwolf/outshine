#include "PlanarMeshInternal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <numeric>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::PlanarMeshDetail {
namespace {
uint32_t Root(std::vector<uint32_t> &parents, uint32_t at) {
  while (parents[at] != at) {
    parents[at] = parents[parents[at]];
    at = parents[at];
  }
  return at;
}

std::expected<void, PlanarMeshError> Join(PlanarMesh &mesh,
                                          const PlanHierarchy &index,
                                          std::span<const PlanarPoint> points,
                                          std::vector<uint32_t> &parents,
                                          double distance,
                                          PlanarMeshBudget budget,
                                          const std::stop_token &stop) {
  for (uint32_t at = 0; at < points.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(PlanarMeshError::Canceled); }
    const auto emit = [&](uint32_t other) {
      if (mesh.Work.WeldCandidates == budget.Candidates) { return false; }
      ++mesh.Work.WeldCandidates;
      if (other <= at || std::hypot(points[at][0] - points[other][0],
                                    points[at][1] - points[other][1]) > distance) {
        return true;
      }
      const uint32_t a = Root(parents, at);
      const uint32_t b = Root(parents, other);
      parents[std::max(a, b)] = std::min(a, b);
      return true;
    };
    if (!index.Query(PointBox(points[at], distance), emit)) {
      return std::unexpected(PlanarMeshError::CapacityExceeded);
    }
  }
  return {};
}
}

Box PointBox(PlanarPoint point, double reach) {
  return {.Min = {{point[0] - reach, point[1] - reach, 0}},
          .Max = {{point[0] + reach, point[1] + reach, 0}}};
}

double Area(PlanarPoint a, PlanarPoint b, PlanarPoint c) {
  return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

std::expected<void, PlanarMeshError> Weld(PlanarMesh &mesh,
                                          std::span<const PlanarPoint> points,
                                          double distance,
                                          PlanarMeshBudget budget,
                                          const std::stop_token &stop) {
  std::vector<Box> boxes;
  boxes.reserve(points.size());
  for (const auto point : points) { boxes.push_back(PointBox(point, 0)); }
  const auto index = PlanHierarchy::Build(boxes);
  if (!index) { return std::unexpected(PlanarMeshError::InvalidInput); }
  std::vector<uint32_t> parents(points.size());
  std::ranges::iota(parents, 0u);
  if (auto joined = Join(mesh, *index, points, parents, distance, budget, stop); !joined) {
    return joined;
  }
  mesh.PointRemap.resize(points.size());
  for (uint32_t at = 0; at < points.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(PlanarMeshError::Canceled); }
    const uint32_t root = Root(parents, at);
    if (root == at) {
      mesh.PointRemap[at] = static_cast<uint32_t>(mesh.Points.size());
      mesh.Points.push_back(points[at]);
    } else {
      const double drift =
          std::hypot(points[at][0] - points[root][0], points[at][1] - points[root][1]);
      if (drift > distance) { return std::unexpected(PlanarMeshError::WeldDistanceExceeded); }
      mesh.Work.MaximumWeldDistance = std::max(mesh.Work.MaximumWeldDistance, drift);
      mesh.PointRemap[at] = mesh.PointRemap[root];
    }
  }
  return {};
}
}
