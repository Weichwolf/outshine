#include "RoofContourInternal.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::RoofContour {
namespace {
constexpr double kPlaneToleranceM = 1e-7;
constexpr double kMaximumCorrectionM = 5e-5;
constexpr double kMinimumHeightToleranceM = 1e-8;
constexpr double kResidualUlpFactor = 64;

struct RidgePoint {
  Vector3 Position;
  size_t Rank;
};

Checked<RidgePoint>
RefinedRidge(Point node, double height, std::span<const size_t> owners, const Input &input) {
  std::vector<Vector3> matrix;
  std::vector<double> rhs;
  double maximumCurrentResidual = 0;
  const Vector3 current{node[0], node[1], height};
  double magnitude =
      std::max({1.0, std::abs(current[0]), std::abs(current[1]), std::abs(current[2])});
  for (const auto owner : owners) {
    matrix.push_back({input.Normals[owner][0], input.Normals[owner][1], -1});
    const double residual = -input.Intercepts[owner] - (Dot(node, input.Normals[owner]) - height);
    rhs.push_back(residual);
    maximumCurrentResidual = std::max(maximumCurrentResidual, std::abs(residual));
    magnitude = std::max(magnitude, std::abs(input.Intercepts[owner]));
  }
  const auto solved = SolveSmall(matrix, rhs);
  Vector3 solution;
  for (size_t axis = 0; axis < 3; ++axis) {
    solution[axis] = current[axis] + solved.Solution[axis];
  }
  double solutionResidual = 0;
  for (const auto owner : owners) {
    solutionResidual = std::max(solutionResidual,
                                std::abs(input.Normals[owner][0] * solution[0] +
                                         input.Normals[owner][1] * solution[1] - solution[2] +
                                         input.Intercepts[owner]));
  }
  if (solved.Rank < 2 || solutionResidual > kPlaneToleranceM) {
    return std::unexpected(Failure{"incompatible ridge planes"});
  }
  if (maximumCurrentResidual <=
      kResidualUlpFactor * std::numeric_limits<double>::epsilon() * magnitude) {
    solution = current;
  }
  return RidgePoint{.Position = solution, .Rank = solved.Rank};
}

Checked<double> CollinearHeight(Point node, std::span<const size_t> owners, const Input &input) {
  double minimum = std::numeric_limits<double>::infinity();
  double maximum = -minimum;
  double total = 0;
  for (const auto owner : owners) {
    const double height = Dot(node, input.Normals[owner]) + input.Intercepts[owner];
    minimum = std::min(minimum, height);
    maximum = std::max(maximum, height);
    total += height;
  }
  if (maximum - minimum > kPlaneToleranceM) {
    return std::unexpected(Failure{"collinear facet height mismatch"});
  }
  return total / static_cast<double>(owners.size());
}
}

Checked<double> RefineRidges(Partition &plan, const Input &input) {
  auto nodes = plan.Nodes;
  auto heights = plan.Distances;
  const auto faces = plan.Faces;
  const auto ownerIds = plan.Ownership;
  std::vector<std::vector<size_t>> incident(nodes.size());
  for (size_t face = 0; face < faces.size(); ++face) {
    for (const auto node : faces[face]) { incident[node].push_back(ownerIds[face]); }
  }
  double displacement = 0;
  for (size_t at = 0; at < nodes.size(); ++at) {
    if (heights[at] == 0) { continue; }
    const auto solved = RefinedRidge(nodes[at], heights[at], incident[at], input);
    if (!solved) { return std::unexpected(solved.error()); }
    const auto &solution = solved->Position;
    if (solved->Rank == 2) {
      const auto height = CollinearHeight(nodes[at], incident[at], input);
      if (!height) { return std::unexpected(height.error()); }
      heights[at] = *height;
    } else {
      const auto delta = Subtract(nodes[at], {solution[0], solution[1]});
      displacement = std::max(displacement, std::hypot(delta[0], delta[1]));
      nodes[at] = {solution[0], solution[1]};
      heights[at] = solution[2];
    }
    if (!std::isfinite(nodes[at][0]) || !std::isfinite(nodes[at][1]) ||
        !std::isfinite(heights[at]) || heights[at] < -kMinimumHeightToleranceM) {
      return std::unexpected(Failure{"invalid refined node"});
    }
    heights[at] = std::max(0.0, heights[at]);
  }
  if (displacement > kMaximumCorrectionM ||
      displacement + plan.MaximumInterfaceRefinementM > kMaximumCorrectionM) {
    return std::unexpected(Failure{"combined metric correction bound"});
  }
  plan.Nodes = std::move(nodes);
  plan.Distances = std::move(heights);
  return displacement;
}

}
