#include "HeightSolidInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <ranges>
#include <span>
#include <vector>

namespace outshine::HeightSolids {
namespace {
std::span<const HeightNode> Between(State &state, uint32_t base, std::array<double, 2> interval) {
  ++state.Output.Work.WallQueries;
  const auto row = std::span(state.Heights)
                       .subspan(state.Offsets[base],
                                state.Offsets[static_cast<size_t>(base) + 1] - state.Offsets[base]);
  const auto [low, high] = interval;
  if (low >= high) { return {}; }
  const auto less = [&](double left, double right) {
    ++state.Output.Work.HeightComparisons;
    return left < right;
  };
  const auto begin = std::ranges::upper_bound(row, low, less, &HeightNode::Height);
  const auto end = std::ranges::lower_bound(
      std::ranges::subrange(begin, row.end()), high, less, &HeightNode::Height);
  const auto count = static_cast<size_t>(end - begin);
  state.Output.Work.IntermediateHeights += count;
  return {begin, count};
}

Checked<void> Emit(State &state, PlanarFace face, uint32_t parent, HeightSolidWallKind kind) {
  if (state.Output.Faces.size() >= state.Budget.Faces ||
      state.Output.Faces.size() >= std::numeric_limits<uint32_t>::max() / 3) {
    return std::unexpected(Error::CapacityExceeded);
  }
  state.Output.Faces.push_back(face);
  state.Output.WallParents.push_back(parent);
  state.Output.WallKinds.push_back(kind);
  return {};
}

Checked<uint32_t> Centre(State &state, std::span<const uint32_t> rim) {
  if (state.Output.Points.size() >= state.Budget.Vertices ||
      state.Output.Points.size() >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  std::array<double, 3> point{};
  for (const auto index : rim) {
    for (size_t axis = 0; axis < point.size(); ++axis) {
      point[axis] += state.Output.Points[index][axis];
    }
  }
  for (auto &value : point) {
    value /= static_cast<double>(rim.size());
    if (!std::isfinite(value)) { return std::unexpected(Error::Unrepresentable); }
  }
  const auto index = static_cast<uint32_t>(state.Output.Points.size());
  state.Output.Points.push_back(point);
  return index;
}

Checked<void> EdgeWall(State &state, Incidence first, Incidence second) {
  const auto a = state.Input.Faces[first.Face];
  const auto b = state.Input.Faces[second.Face];
  if (a[first.Start] != b[second.End] || a[first.End] != b[second.Start]) {
    return std::unexpected(Error::NonManifold);
  }
  auto top = state.Output.Faces[first.Face];
  auto other = state.Output.Faces[second.Face];
  if (top[first.Start] == other[second.End] && top[first.End] == other[second.Start]) { return {}; }
  const double startHeight = state.Output.Points[top[first.Start]][2];
  const double otherStart = state.Output.Points[other[second.End]][2];
  const double endHeight = state.Output.Points[top[first.End]][2];
  const double otherEnd = state.Output.Points[other[second.Start]][2];
  if (startHeight < otherStart || (startHeight == otherStart && endHeight < otherEnd)) {
    std::swap(first, second);
    std::swap(top, other);
  }
  if (state.Output.Points[top[first.Start]][2] < state.Output.Points[other[second.End]][2] ||
      state.Output.Points[top[first.End]][2] < state.Output.Points[other[second.Start]][2]) {
    return std::unexpected(Error::CrossingHeights);
  }
  return AddWall(
      state,
      {top[first.End], top[first.Start], other[second.End], other[second.Start]},
      {state.Input.Faces[first.Face][first.Start], state.Input.Faces[first.Face][first.End]},
      first.Face,
      HeightSolidWallKind::Discontinuity);
}
}

Checked<void> AddWall(State &state,
                      const std::array<uint32_t, 4> &corners,
                      std::array<uint32_t, 2> bases,
                      uint32_t parent,
                      HeightSolidWallKind kind) {
  const auto atFirst = Between(
      state, bases[0], {state.Output.Points[corners[2]][2], state.Output.Points[corners[1]][2]});
  const auto atSecond = Between(
      state, bases[1], {state.Output.Points[corners[3]][2], state.Output.Points[corners[0]][2]});
  std::vector<uint32_t> rim;
  rim.reserve(corners.size() + atFirst.size() + atSecond.size());
  rim.push_back(corners[0]);
  rim.push_back(corners[1]);
  for (const auto node : std::views::reverse(atFirst)) { rim.push_back(node.Vertex); }
  rim.push_back(corners[2]);
  rim.push_back(corners[3]);
  for (const auto node : atSecond) { rim.push_back(node.Vertex); }
  const auto repeated = std::ranges::unique(rim);
  rim.erase(repeated.begin(), repeated.end());
  if (rim.size() > 1 && rim.front() == rim.back()) { rim.pop_back(); }
  if (rim.size() == 3) { return Emit(state, {rim[0], rim[1], rim[2]}, parent, kind); }
  if (rim.size() == 4) {
    if (auto emitted = Emit(state, {rim[0], rim[1], rim[2]}, parent, kind); !emitted) {
      return emitted;
    }
    return Emit(state, {rim[0], rim[2], rim[3]}, parent, kind);
  }
  if (rim.size() < 3) { return std::unexpected(Error::Unrepresentable); }
  const auto centre = Centre(state, rim);
  if (!centre) { return std::unexpected(centre.error()); }
  for (size_t at = 0; at < rim.size(); ++at) {
    if (auto emitted = Emit(state, {*centre, rim[at], rim[(at + 1) % rim.size()]}, parent, kind);
        !emitted) {
      return emitted;
    }
  }
  return {};
}

Checked<void> CloseWalls(State &state) {
  for (const auto &edge : state.Edges) {
    if (auto valid = state.Check(); !valid) { return valid; }
    const auto first = edge.Faces[0];
    if (edge.Count == 1) {
      const auto top = state.Output.Faces[first.Face];
      const auto bottom = state.Bottom[first.Face];
      if (auto wall =
              AddWall(state,
                      {top[first.End], top[first.Start], bottom[first.Start], bottom[first.End]},
                      {state.Input.Faces[first.Face][first.Start],
                       state.Input.Faces[first.Face][first.End]},
                      first.Face,
                      HeightSolidWallKind::Boundary);
          !wall) {
        return wall;
      }
    } else {
      if (auto wall = EdgeWall(state, first, edge.Faces[1]); !wall) { return wall; }
    }
  }
  return {};
}
}
