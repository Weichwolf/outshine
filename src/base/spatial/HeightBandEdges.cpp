#include "HeightBandInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>

namespace outshine::HeightBands {
namespace {
VertexPair Root(const State &state, uint32_t vertex) {
  if (vertex < state.Input.Points.size()) { return {vertex, vertex}; }
  return state.Roots[vertex - state.Input.Points.size()];
}

Checked<VertexPair> ParentEdge(const State &state, VertexPair vertices) {
  const auto a = Root(state, vertices[0]);
  const auto b = Root(state, vertices[1]);
  const uint32_t low = std::min(a[0], b[0]);
  const uint32_t high = std::max(a[1], b[1]);
  if (low == high) { return std::unexpected(Error::Unrepresentable); }
  for (const auto vertex : std::array{a[0], a[1], b[0], b[1]}) {
    if (vertex != low && vertex != high) { return std::unexpected(Error::Unrepresentable); }
  }
  return VertexPair{low, high};
}

Checked<std::array<double, 3>> Interpolate(const State &state, EdgeLevel key) {
  const auto a = state.Input.Points[key.Edge[0]];
  const auto b = state.Input.Points[key.Edge[1]];
  const double level = state.Input.Levels[key.Level];
  const double ratio = (level - a[2]) / (b[2] - a[2]);
  if (!std::isfinite(ratio) || ratio <= 0 || ratio >= 1) {
    return std::unexpected(Error::Unrepresentable);
  }
  std::array<double, 3> point{};
  for (size_t axis = 0; axis < point.size(); ++axis) {
    point[axis] = a[axis] + ratio * (b[axis] - a[axis]);
    if (!std::isfinite(point[axis])) { return std::unexpected(Error::Unrepresentable); }
  }
  point[2] = level;
  return point;
}
}

Checked<uint32_t> Crossing(State &state, VertexPair edge, uint32_t level) {
  ++state.Output.Work.IntersectionQueries;
  for (const auto vertex : edge) {
    if (state.Output.Points[vertex][2] == state.Input.Levels[level]) { return vertex; }
  }
  const auto parent = ParentEdge(state, edge);
  if (!parent) { return std::unexpected(parent.error()); }
  const EdgeLevel key{.Edge = *parent, .Level = level};
  if (const auto found = state.Intersections.find(key); found != state.Intersections.end()) {
    return found->second;
  }
  if (state.Output.Points.size() >= state.Budget.Points ||
      state.Output.Points.size() >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  const auto point = Interpolate(state, key);
  if (!point) { return std::unexpected(point.error()); }
  if (state.Intersections.empty()) {
    state.Intersections.reserve(
        std::min(state.Input.Faces.size() * 2, state.Budget.Points - state.Input.Points.size()));
  }
  const auto index = static_cast<uint32_t>(state.Output.Points.size());
  state.Output.Points.push_back(*point);
  state.Roots.push_back(*parent);
  state.Intersections.emplace(key, index);
  ++state.Output.Work.NewIntersections;
  return index;
}
}
