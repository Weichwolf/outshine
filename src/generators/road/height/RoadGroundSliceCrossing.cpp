#include "RoadGroundMeshInternal.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>

namespace outshine::Generators::RoadGround {
namespace {
MeshResult<uint32_t> Intersection(SliceState &state, SliceEdge edge, size_t knot) {
  const auto found = std::ranges::lower_bound(state.Edges, edge);
  if (found == state.Edges.end() || *found != edge) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  auto &cached =
      state.Nodes[static_cast<size_t>(found - state.Edges.begin()) * state.Knots.size() + knot];
  ++state.Work.EdgeQueries;
  if (cached != std::numeric_limits<uint32_t>::max()) { return cached; }
  const auto &first = state.Source.Priors[edge.First].At;
  const auto &last = state.Source.Priors[edge.Last].At;
  const PlanarPoint a{first.EastM, first.NorthM};
  const PlanarPoint b{last.EastM, last.NorthM};
  const auto &patch = state.Source.Patches[state.Patch];
  const auto start = ParameterAt(patch, a, state.ToleranceM);
  const auto end = ParameterAt(patch, b, state.ToleranceM);
  if (!start || !end) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  const double level = state.Knots[knot];
  const double denominator = end->U - start->U - level * (end->D - start->D);
  if (denominator == 0) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  const double fraction = std::clamp((level * start->D - start->U) / denominator, 0., 1.);
  const PlanarPoint point{a[0] + fraction * (b[0] - a[0]), a[1] + fraction * (b[1] - a[1])};
  if (!std::isfinite(fraction)) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  if (std::hypot(point[0] - a[0], point[1] - a[1]) <= state.ToleranceM) {
    cached = edge.First;
  } else if (std::hypot(point[0] - b[0], point[1] - b[1]) <= state.ToleranceM) {
    cached = edge.Last;
  } else {
    if (state.Mesh.Points.size() >= state.Budget.Vertices) {
      return std::unexpected(RoadGroundError::CapacityExceeded);
    }
    cached = static_cast<uint32_t>(state.Mesh.Points.size());
    state.Mesh.Points.push_back(point);
  }
  return cached;
}
}

MeshResult<SliceNode>
Crossing(SliceState &state, const PlanarFace &original, SliceNode a, SliceNode b, size_t knot) {
  const double level = state.Knots[knot];
  if (a.Parameter == level) { return a; }
  if (b.Parameter == level) { return b; }
  unsigned support = a.Support | b.Support;
  if (std::popcount(support) != 2) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  const auto first = original[std::countr_zero(support)];
  const auto last = original[std::countr_zero(support & (support - 1))];
  const auto vertex =
      Intersection(state, {.First = std::min(first, last), .Last = std::max(first, last)}, knot);
  if (!vertex) { return std::unexpected(vertex.error()); }
  for (size_t corner = 0; corner < 3; ++corner) {
    if (original[corner] == *vertex) { support = 1u << corner; }
  }
  return SliceNode{.Vertex = *vertex, .Parameter = level, .Support = support};
}
}
