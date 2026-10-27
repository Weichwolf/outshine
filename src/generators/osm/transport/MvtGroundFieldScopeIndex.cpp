#include "MvtGroundFieldScopeInternal.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <tuple>

namespace outshine::Generators::Osm::GroundFieldScopes {
namespace {
Checked<void> Candidate(State &state) {
  const auto &source = state.Source();
  const auto &graph = source.Source->Topology();
  const auto nodes = graph.Nodes();
  const auto edges = graph.Edges();
  state.NodeOwners.assign(nodes.size(), kAbsent);
  state.EdgeOwners.assign(edges.size(), kAbsent);
  state.Groups.assign(source.Nodes.size(), kAbsent);
  state.Seen.assign(source.SourceEdges.size() * 2, 0);
  state.KnownUnknown.assign(source.Nodes.size(), 0);
  for (uint32_t at = 0; at < source.Nodes.size(); ++at) {
    const auto node = source.Nodes[at];
    if (node >= nodes.size() || state.NodeOwners[node] != kAbsent) {
      return std::unexpected(Error::InvalidCandidate);
    }
    state.NodeOwners[node] = at;
  }
  for (uint32_t at = 0; at < source.SourceEdges.size(); ++at) {
    if (state.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto edge = source.SourceEdges[at];
    if (edge >= edges.size() || state.EdgeOwners[edge] != kAbsent) {
      return std::unexpected(Error::InvalidCandidate);
    }
    const auto &value = edges[edge];
    const auto a = nodes[source.Nodes[at]].SourceNodeId;
    const auto b = nodes[source.Nodes[at + 1]].SourceNodeId;
    if (value.Bridge || value.Tunnel || value.Layer != 0 || value.WidthM != source.WidthM ||
        ((value.FromNodeId != a || value.ToNodeId != b) &&
         (value.FromNodeId != b || value.ToNodeId != a))) {
      return std::unexpected(Error::InvalidCandidate);
    }
    state.EdgeOwners[edge] = at;
    for (const int offset : {-1, 1}) {
      const auto alias = static_cast<int64_t>(edge) + offset;
      if (alias >= 0 && static_cast<size_t>(alias) < edges.size() &&
          edges[static_cast<size_t>(alias)].Id.PathId == value.Id.PathId &&
          edges[static_cast<size_t>(alias)].Id.SegmentOrdinal == value.Id.SegmentOrdinal) {
        state.EdgeOwners[static_cast<size_t>(alias)] = at;
      }
    }
  }
  return {};
}

Checked<void> Groups(State &state) {
  const auto groups = state.Inputs.OriginalGroups();
  for (size_t at = 0; at < state.Source().Nodes.size(); ++at) {
    if (state.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    ++state.Output.Work.OriginalNodeLookups;
    const auto group = groups[state.Source().Nodes[at]];
    if (group == MvtGroundFieldCandidateInputs::MissingGroup) {
      return std::unexpected(Error::IncompleteIncidence);
    }
    if (group == MvtGroundFieldCandidateInputs::AmbiguousGroup ||
        group >= state.Facts.Groups.size()) {
      return std::unexpected(Error::InvalidBinding);
    }
    state.Groups[at] = group;
  }
  std::ranges::sort(state.Groups);
  const auto tail = std::ranges::unique(state.Groups);
  state.Groups.erase(tail.begin(), tail.end());
  return {};
}
}

Checked<void> Index(State &state) {
  if (auto candidate = Candidate(state); !candidate) { return candidate; }
  state.Output.Work.ContainerCapacityBytes =
      (state.NodeOwners.capacity() + state.EdgeOwners.capacity() + state.Groups.capacity() +
       state.Source().Nodes.capacity() + state.Source().SourceEdges.capacity()) *
          sizeof(uint32_t) +
      state.Seen.capacity() + state.KnownUnknown.capacity();
  return Groups(state);
}
}
