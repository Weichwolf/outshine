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
  const auto &graph = state.Source().Source->Topology();
  size_t next = 0;
  for (uint32_t at = 0; at < state.Facts.Groups.size(); ++at) {
    const auto &group = state.Facts.Groups[at];
    if (group.FirstMember != next || group.MemberCount > state.Facts.Members.size() - next) {
      return std::unexpected(Error::InvalidBinding);
    }
    const auto &partition = state.Facts.NodeGroups->Groups[at];
    const auto &ids = state.Facts.NodeGroups->Nodes;
    if (partition.FirstNode > ids.size() ||
        partition.NodeCount > ids.size() - partition.FirstNode) {
      return std::unexpected(Error::InvalidBinding);
    }
    for (size_t member = next; member < next + group.MemberCount; ++member) {
      if (state.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
      const auto &value = state.Facts.Members[member];
      const auto *node = graph.FindNode(value.DomainNode);
      if (node == nullptr || value.NodeGroup != at || value.End > 1) {
        return std::unexpected(Error::InvalidBinding);
      }
      ++state.Output.Work.OriginalMemberVisits;
      const auto owner = state.NodeOwners[static_cast<size_t>(node - graph.Nodes().data())];
      if (owner == kAbsent) { continue; }
      if (state.Groups[owner] != kAbsent && state.Groups[owner] != at) {
        return std::unexpected(Error::InvalidBinding);
      }
      state.Groups[owner] = at;
    }
    next += group.MemberCount;
  }
  if (next != state.Facts.Members.size() ||
      std::ranges::find(state.Groups, kAbsent) != state.Groups.end()) {
    return std::unexpected(Error::IncompleteIncidence);
  }
  std::ranges::sort(state.Groups);
  const auto tail = std::ranges::unique(state.Groups);
  state.Groups.erase(tail.begin(), tail.end());
  return {};
}
}

Checked<void> Index(State &state) {
  if (auto candidate = Candidate(state); !candidate) { return candidate; }
  state.Ways.assign(state.Snapshot.SourceWayCount, kAbsent);
  for (uint32_t at = 0; at < state.Facts.Ways.size(); ++at) {
    const auto way = state.Facts.Ways[at].StreetWay;
    if (way >= state.Ways.size() || state.Ways[way] != kAbsent ||
        state.Facts.Ways[at].PointCount < 2) {
      return std::unexpected(Error::InvalidBinding);
    }
    state.Ways[way] = at;
  }
  state.Output.Work.ContainerCapacityBytes =
      (state.NodeOwners.capacity() + state.EdgeOwners.capacity() + state.Ways.capacity() +
       state.Groups.capacity() + state.Source().Nodes.capacity() +
       state.Source().SourceEdges.capacity()) *
          sizeof(uint32_t) +
      state.Seen.capacity() + state.KnownUnknown.capacity();
  return Groups(state);
}
}
