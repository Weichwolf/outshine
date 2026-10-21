#include "RoofContourInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <map>
#include <utility>
#include <vector>

namespace outshine::Generators::RoofContour {
namespace {
constexpr double kPlaneToleranceM = 1e-7;
constexpr double kAreaAbsoluteToleranceM2 = 1e-7;
constexpr double kMinimumFacetAreaM2 = 1e-10;
constexpr double kMinimumRiseM = 1e-10;
constexpr double kRelativeAreaTolerance = 1e-10;
using Edge = std::array<size_t, 2>;

Checked<Ring>
FacetPoints(const Partition &plan, const Input &input, size_t at, std::map<Edge, size_t> &edges) {
  std::vector<Point> points;
  const auto owner = plan.Ownership[at];
  for (size_t vertex = 0; vertex < plan.Faces[at].size(); ++vertex) {
    const auto node = plan.Faces[at][vertex];
    points.push_back(plan.Nodes[node]);
    if (std::abs(Dot(plan.Nodes[node], input.Normals[owner]) + input.Intercepts[owner] -
                 plan.Distances[node]) > kPlaneToleranceM) {
      return std::unexpected(Failure{"facet plane mismatch"});
    }
    Edge edge{node, plan.Faces[at][(vertex + 1) % plan.Faces[at].size()]};
    std::ranges::sort(edge);
    ++edges[edge];
  }
  return points;
}

Checked<void> ValidateCoverage(const Geos &g,
                               const GEOSGeometry *footprint,
                               std::vector<Geometry> polygons,
                               double sumArea) {
  const auto collection = Collection(g, std::move(polygons));
  if (!collection) { return std::unexpected(Failure{"final facet collection"}); }
  const auto coverage = g.Own(GEOSUnaryUnion_r(g.Handle, collection.get()));
  const auto difference =
      g.Own(coverage ? GEOSSymDifference_r(g.Handle, coverage.get(), footprint) : nullptr);
  double area = 0;
  double coverageArea = 0;
  double differenceArea = 0;
  if (!coverage || !difference || (GEOSArea_r(g.Handle, footprint, &area) == 0) ||
      (GEOSArea_r(g.Handle, coverage.get(), &coverageArea) == 0) ||
      (GEOSArea_r(g.Handle, difference.get(), &differenceArea) == 0)) {
    return std::unexpected(Failure{"final coverage measure"});
  }
  const double guard = std::max(kAreaAbsoluteToleranceM2, area * kRelativeAreaTolerance);
  if (differenceArea > guard || sumArea - coverageArea > guard) {
    return std::unexpected(Failure{"final coverage gap/overlap/courtyard"});
  }
  return {};
}
}

Checked<void> ValidatePartition(const Geos &g,
                                const GEOSGeometry *footprint,
                                const Partition &plan,
                                const Input &input) {
  const auto &nodes = plan.Nodes;
  const auto &heights = plan.Distances;
  const auto &faces = plan.Faces;
  std::map<Edge, size_t> edges;
  std::vector<Geometry> polygons;
  double sumArea = 0;
  size_t corners = 0;
  for (size_t at = 0; at < faces.size(); ++at) {
    auto facePoints = FacetPoints(plan, input, at, edges);
    if (!facePoints) { return std::unexpected(facePoints.error()); }
    const auto &points = *facePoints;
    corners += points.size();
    if (corners > input.Limits.Corners) {
      return std::unexpected(Failure{HipRoofError::CapacityExceeded, "facet corner budget"});
    }
    auto polygon = Polygon(g, points);
    double area = 0;
    if (!polygon || GEOSisValid_r(g.Handle, polygon.get()) != 1 ||
        (GEOSArea_r(g.Handle, polygon.get(), &area) == 0) || area <= kMinimumFacetAreaM2) {
      return std::unexpected(Failure{"invalid final facet"});
    }
    sumArea += area;
    polygons.push_back(std::move(polygon));
  }
  for (const auto &[edge, count] : edges) {
    if (count < 1 || count > 2) { return std::unexpected(Failure{"nonmanifold ridge"}); }
    if (count == 1 && (heights[edge[0]] != 0 || heights[edge[1]] != 0)) {
      return std::unexpected(Failure{"open ridge"});
    }
  }
  const auto covered = ValidateCoverage(g, footprint, std::move(polygons), sumArea);
  if (!covered) { return std::unexpected(covered.error()); }
  if (nodes.empty() || *std::ranges::max_element(heights) <= kMinimumRiseM) {
    return std::unexpected(Failure{"no positive roof rise"});
  }
  return {};
}
}
