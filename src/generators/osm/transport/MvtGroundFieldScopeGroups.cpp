#include "MvtGroundFieldScopeInternal.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <tuple>
#include <vector>

namespace outshine::Generators::Osm::GroundFieldScopes {
namespace {
struct Member {
  const World::TransportEdge *Edge;
  const MvtTransportWayFacts *Way;
  uint32_t Owner;
  uint32_t Node;
  uint8_t PhysicalEnd;
  uint8_t Omissions;
};

TransportSourceMember Portable(const MvtTransportSourceMember &member, const Member &bound) {
  return {.Edge = member.Edge,
          .Node = member.DomainNode,
          .SourceStep = member.SourceStep,
          .PointCount = bound.Way->PointCount,
          .End = member.End,
          .Layer = bound.Edge->Layer,
          .Bridge = bound.Edge->Bridge,
          .Tunnel = bound.Edge->Tunnel,
          .Terminal = member.SourceTerminal,
          .OwnsEnd = bound.Way->OwnsEnds[bound.PhysicalEnd]};
}

Checked<Member> Bound(const State &state, const MvtTransportSourceMember &member) {
  const auto &graph = state.Source().Source->Topology();
  const auto *edge = graph.FindEdge(member.Edge);
  const auto *node = graph.FindNode(member.DomainNode);
  if (edge == nullptr || node == nullptr || member.End > 1 ||
      (member.End == 0 ? edge->FromNodeId : edge->ToNodeId) != member.DomainNode ||
      member.StreetWay >= state.Ways.size() || state.Ways[member.StreetWay] == kAbsent) {
    return std::unexpected(Error::InvalidBinding);
  }
  const auto &way = state.Facts.Ways[state.Ways[member.StreetWay]];
  const auto &provenance = state.Source().Source->Provenance();
  const auto path = std::ranges::lower_bound(
      provenance.Paths, member.Edge.PathId, {}, &World::TransportPathSource::PathId);
  if (path == provenance.Paths.end() || path->PathId != member.Edge.PathId ||
      path->ReferenceCount != 1 || path->FirstReference >= provenance.References.size() ||
      state.Snapshot.StreetWays[static_cast<size_t>(path - provenance.Paths.begin())] !=
          member.StreetWay) {
    return std::unexpected(Error::InvalidBinding);
  }
  const auto &reference = provenance.References[path->FirstReference];
  const auto physical =
      static_cast<uint8_t>(member.End ^ static_cast<uint8_t>(member.Edge.Direction));
  if (member.SourceStep != reference.SegmentOrdinal || member.SourceStep >= way.PointCount - 1 ||
      (member.SourceTerminal &&
       (physical == 0 ? member.SourceStep != 0 : member.SourceStep != way.PointCount - 2))) {
    return std::unexpected(Error::InvalidBinding);
  }
  const auto &partition = state.Facts.NodeGroups->Groups[member.NodeGroup];
  const auto domain = state.Facts.NodeGroups->NodesOf(partition);
  if (std::ranges::find(domain, member.DomainNode) == domain.end()) {
    return std::unexpected(Error::InvalidBinding);
  }
  return Member{
      .Edge = edge,
      .Way = &way,
      .Owner = state.EdgeOwners[static_cast<size_t>(edge - graph.Edges().data())],
      .Node = static_cast<uint32_t>(node - graph.Nodes().data()),
      .PhysicalEnd = physical,
      .Omissions =
          state.Inputs.PathOmissions()[static_cast<size_t>(path - provenance.Paths.begin())]};
}

Checked<void> Ground(State &state, const Member &bound) {
  const auto width = static_cast<double>(std::bit_cast<float>(bound.Way->HalfWidthWord)) * 2;
  const auto gradient = static_cast<double>(std::bit_cast<float>(bound.Way->MaximumGradientWord));
  if (width != state.Source().WidthM || gradient != state.Source().MaximumGradient ||
      !std::isfinite(gradient) || gradient <= 0 || bound.Edge->WidthM != width) {
    return std::unexpected(Error::MissingProfile);
  }
  const size_t endpoint = static_cast<size_t>(bound.Owner) * 2 + bound.PhysicalEnd;
  if (state.Seen[endpoint] != 0) { return std::unexpected(Error::InvalidBinding); }
  state.Seen[endpoint] = 1;
  if ((bound.Omissions & (1U << 4U)) != 0) {
    const auto node = state.NodeOwners[bound.Node];
    if (node == kAbsent) { return std::unexpected(Error::InvalidBinding); }
    state.KnownUnknown[node] = 1;
  }
  return {};
}

Checked<void> External(State &state,
                       uint32_t group,
                       const MvtTransportSourceMember &member,
                       const Member &bound) {
  const auto &edge = *bound.Edge;
  if ((!edge.Bridge && edge.Layer == 0) || (edge.Tunnel && edge.Layer == 0) ||
      !member.SourceTerminal) {
    return std::unexpected(Error::UnknownExit);
  }
  state.Output.Field.Info.Exits.push_back({.SourceGroup = group,
                                           .Ground = {},
                                           .External = member.Edge,
                                           .ExternalNode = member.DomainNode,
                                           .GroundEnd = 0,
                                           .ExternalEnd = member.End,
                                           .Layer = edge.Layer,
                                           .Bridge = edge.Bridge,
                                           .Tunnel = edge.Tunnel});
  return {};
}

Checked<void> JoinExits(State &state,
                        size_t groundCount,
                        const MvtTransportSourceMember &ground,
                        size_t firstExit) {
  auto &exits = state.Output.Field.Info.Exits;
  if (firstExit == exits.size()) { return {}; }
  const auto &graph = state.Source().Source->Topology();
  const auto *groundNode = graph.FindNode(ground.DomainNode);
  const auto owner = state.NodeOwners[static_cast<size_t>(groundNode - graph.Nodes().data())];
  if (groundCount != 1 || !ground.SourceTerminal ||
      (owner != 0 && owner + 1 != state.Source().Nodes.size())) {
    return std::unexpected(Error::UnknownExit);
  }
  for (auto &exit : std::span(exits).subspan(firstExit)) {
    exit.Ground = ground.Edge;
    exit.GroundEnd = ground.End;
  }
  return {};
}

Checked<void>
Members(State &state, uint32_t index, std::span<const MvtTransportSourceMember> members) {
  const MvtTransportSourceMember *ground = nullptr;
  size_t groundCount = 0;
  const size_t firstExit = state.Output.Field.Info.Exits.size();
  std::vector<TransportSourceMember> portable;
  portable.reserve(members.size());
  std::array<uint32_t, 2> grounds{};
  state.Output.Work.EndScratchCapacityBytes =
      std::max(state.Output.Work.EndScratchCapacityBytes,
               portable.capacity() * sizeof(TransportSourceMember));
  for (const auto &member : members) {
    if (state.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (state.Output.Work.OriginalMemberVisits + state.Output.Work.QualifiedMemberVisits +
            state.Output.Work.EndRelationVisits + state.Output.Work.ContentMemberVisits >=
        state.Limits.MemberVisits) {
      return std::unexpected(Error::CapacityExceeded);
    }
    ++state.Output.Work.QualifiedMemberVisits;
    const auto bound = Bound(state, member);
    if (!bound) { return std::unexpected(bound.error()); }
    if ((bound->Omissions & (1U << 5U)) != 0) { return std::unexpected(Error::MissingTopology); }
    if (member.SourceTerminal && !bound->Way->OwnsEnds[bound->PhysicalEnd]) {
      return std::unexpected(Error::Frontier);
    }
    if (bound->Owner != kAbsent) {
      if (groundCount == grounds.size()) { return std::unexpected(Error::InvalidCandidate); }
      grounds[groundCount] = static_cast<uint32_t>(portable.size());
      ground = &member;
      ++groundCount;
      if (auto checked = Ground(state, *bound); !checked) { return checked; }
    } else if (auto checked = External(state, index, member, *bound); !checked) {
      return checked;
    }
    portable.push_back(Portable(member, *bound));
  }
  if (ground == nullptr || (members.size() == 1 && !ground->SourceTerminal)) {
    return std::unexpected(Error::IncompleteIncidence);
  }
  if (auto joined = JoinExits(state, groundCount, *ground, firstExit); !joined) { return joined; }
  return AppendEnds(state, portable, std::span(grounds).first(groundCount));
}

Checked<void> Group(State &state, uint32_t index) {
  const auto &group = state.Facts.Groups[index];
  if (!group.Complete || group.FilteredEndpointCount != 0 ||
      group.OriginalEndpointCount != group.MemberCount) {
    return std::unexpected(Error::IncompleteIncidence);
  }
  if (group.Frontier) { return std::unexpected(Error::Frontier); }
  return Members(
      state, index, std::span(state.Facts.Members).subspan(group.FirstMember, group.MemberCount));
}
}

Checked<void> Qualify(State &state) {
  size_t members = 0;
  for (const auto group : state.Groups) {
    if (state.Facts.Groups[group].MemberCount > state.Limits.MemberVisits - members) {
      return std::unexpected(Error::CapacityExceeded);
    }
    members += state.Facts.Groups[group].MemberCount;
  }
  const size_t bytes = state.Output.Work.ContainerCapacityBytes;
  constexpr size_t perMember = sizeof(GroundPortFieldExit) + sizeof(TransportSourceEndTopology) +
                               2 * sizeof(TransportSourceMember);
  if (bytes > state.Limits.ContainerBytes ||
      members > (state.Limits.ContainerBytes - bytes) / perMember) {
    return std::unexpected(Error::CapacityExceeded);
  }
  state.Output.Field.Info.Exits.reserve(members);
  state.Output.Field.Info.Ends.reserve(members);
  for (const auto group : state.Groups) {
    if (auto qualified = Group(state, group); !qualified) { return qualified; }
  }
  std::ranges::sort(state.Output.Field.Info.Ends, {}, [](const auto &end) {
    return TransportSourceMemberKey(end.Ground);
  });
  if (std::ranges::find(state.Seen, uint8_t{0}) != state.Seen.end()) {
    return std::unexpected(Error::IncompleteIncidence);
  }
  const auto &graph = state.Source().Source->Topology();
  for (size_t owner = 0; owner < state.Source().Nodes.size(); ++owner) {
    const auto id = graph.Nodes()[state.Source().Nodes[owner]].SourceNodeId;
    if (std::ranges::binary_search(state.Inputs.Ports().UnknownNodes, id) &&
        state.KnownUnknown[owner] == 0) {
      return std::unexpected(Error::Frontier);
    }
  }
  state.Output.Work.ContainerCapacityBytes +=
      state.Output.Field.Info.Exits.capacity() * sizeof(GroundPortFieldExit);
  state.Output.Work.ContainerCapacityBytes +=
      state.Output.Field.Info.Ends.capacity() * sizeof(TransportSourceEndTopology);
  for (const auto &end : state.Output.Field.Info.Ends) {
    state.Output.Work.ContainerCapacityBytes +=
        end.Relations.capacity() * sizeof(TransportSourceMember);
  }
  return {};
}
}
