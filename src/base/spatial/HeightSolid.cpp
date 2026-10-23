#include "HeightSolidInternal.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <span>
#include <stop_token>
#include <unordered_map>
#include <utility>
#include <vector>

namespace outshine::HeightSolids {
Checked<void> State::Check() const {
  if (Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  return {};
}

Checked<void> State::VisitEdge() {
  if (auto valid = Check(); !valid) { return valid; }
  if (Output.Work.EdgeVisits >= Budget.EdgeVisits) {
    return std::unexpected(Error::CapacityExceeded);
  }
  ++Output.Work.EdgeVisits;
  return {};
}

namespace {
struct NodeKey {
  uint32_t Point;
  double Height;
  bool operator==(const NodeKey &) const = default;
};

struct NodeHash {
  size_t operator()(const NodeKey &key) const {
    return std::hash<double>{}(key.Height) ^ std::hash<uint32_t>{}(key.Point);
  }
};

Checked<uint32_t> Node(State &state,
                       std::unordered_map<NodeKey, uint32_t, NodeHash> &lookup,
                       uint32_t point,
                       double height) {
  ++state.Output.Work.NodeQueries;
  const NodeKey key{.Point = point, .Height = height};
  if (const auto found = lookup.find(key); found != lookup.end()) { return found->second; }
  if (state.Output.Points.size() >= state.Budget.Vertices ||
      state.Output.Points.size() >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  const auto index = static_cast<uint32_t>(state.Output.Points.size());
  const auto xy = state.Input.Points[point];
  state.Output.Points.push_back({xy[0], xy[1], height});
  state.Bases.push_back(point);
  lookup.emplace(key, index);
  return index;
}

Checked<void> Caps(State &state) {
  std::unordered_map<NodeKey, uint32_t, NodeHash> lookup;
  lookup.reserve(std::min(state.Input.Faces.size() * 6, state.Budget.Vertices));
  state.Output.Faces.reserve(state.Input.Faces.size() * 2);
  state.Bottom.reserve(state.Input.Faces.size());
  for (size_t at = 0; at < state.Input.Faces.size(); ++at) {
    if (auto valid = state.Check(); !valid) { return valid; }
    const auto face = state.Input.Faces[at];
    PlanarFace top{};
    PlanarFace bottom{};
    for (size_t corner = 0; corner < face.size(); ++corner) {
      const auto vertex = Node(state, lookup, face[corner], state.Input.TopHeights[at][corner]);
      if (!vertex) { return std::unexpected(vertex.error()); }
      top[corner] = *vertex;
    }
    for (size_t corner = 0; corner < face.size(); ++corner) {
      const auto vertex =
          Node(state, lookup, face[corner], state.Input.BottomHeights[face[corner]]);
      if (!vertex) { return std::unexpected(vertex.error()); }
      bottom[corner] = *vertex;
    }
    state.Output.Faces.push_back(top);
    state.Bottom.push_back(bottom);
  }
  for (auto face : state.Bottom) {
    std::ranges::reverse(face);
    state.Output.Faces.push_back(face);
  }
  return {};
}

void IndexHeights(State &state) {
  state.Offsets.assign(state.Input.Points.size() + 1, 0);
  for (const auto base : state.Bases) { ++state.Offsets[static_cast<size_t>(base) + 1]; }
  for (size_t at = 1; at < state.Offsets.size(); ++at) {
    state.Offsets[at] += state.Offsets[at - 1];
  }
  auto cursor = state.Offsets;
  state.Heights.resize(state.Bases.size());
  for (uint32_t at = 0; at < state.Bases.size(); ++at) {
    state.Heights[cursor[state.Bases[at]]++] = {.Height = state.Output.Points[at][2], .Vertex = at};
  }
  for (size_t at = 0; at < state.Input.Points.size(); ++at) {
    const auto row = std::span(state.Heights)
                         .subspan(state.Offsets[at], state.Offsets[at + 1] - state.Offsets[at]);
    std::ranges::sort(row, {}, &HeightNode::Height);
  }
  state.Output.Work.IndexedHeights = state.Heights.size();
  state.Output.Work.IndexCapacityBytes =
      state.Offsets.capacity() * sizeof(size_t) + state.Heights.capacity() * sizeof(HeightNode);
}

Checked<void> IndexEdges(State &state) {
  std::unordered_map<uint64_t, size_t> lookup;
  lookup.reserve(state.Input.Faces.size() * 3);
  for (uint32_t at = 0; at < state.Input.Faces.size(); ++at) {
    const auto face = state.Input.Faces[at];
    for (uint32_t corner = 0; corner < face.size(); ++corner) {
      if (auto valid = state.VisitEdge(); !valid) { return valid; }
      const auto next = (corner + 1) % static_cast<uint32_t>(face.size());
      const auto a = face[corner];
      const auto b = face[next];
      const uint64_t key = (uint64_t{std::min(a, b)} << 32) | std::max(a, b);
      const auto [found, inserted] = lookup.try_emplace(key, state.Edges.size());
      if (inserted) { state.Edges.emplace_back(); }
      auto &edge = state.Edges[found->second];
      if (edge.Count == edge.Faces.size()) { return std::unexpected(Error::NonManifold); }
      edge.Faces[edge.Count++] = {.Face = at, .Start = corner, .End = next};
    }
  }
  return {};
}
}
}

namespace outshine {
std::expected<HeightSolid, HeightSolidError> BuildHeightSolid(const HeightSolidInput &input,
                                                              HeightSolidBudget budget,
                                                              const std::stop_token &stop) {
  using namespace HeightSolids;
  if (auto valid = ValidateInput(input, budget, stop); !valid) {
    return std::unexpected(valid.error());
  }
  State state{.Input = input,
              .Budget = budget,
              .Stop = stop,
              .Output = {},
              .Bottom = {},
              .Bases = {},
              .Offsets = {},
              .Heights = {},
              .Edges = {}};
  state.Output.SurfaceFaces = input.Faces.size();
  if (input.Faces.empty()) { return std::move(state.Output); }
  if (auto caps = Caps(state); !caps) { return std::unexpected(caps.error()); }
  IndexHeights(state);
  if (auto edges = IndexEdges(state); !edges) { return std::unexpected(edges.error()); }
  if (auto walls = CloseWalls(state); !walls) { return std::unexpected(walls.error()); }
  if (auto valid = ValidateBody(state); !valid) { return std::unexpected(valid.error()); }
  return std::move(state.Output);
}
}
