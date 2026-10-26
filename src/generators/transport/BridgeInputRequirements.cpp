#include "BridgeInputRequirements.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint8_t kGround = 1;
constexpr uint8_t kRecordedGround = 2;
constexpr uint8_t kExpanded = 4;

struct BridgeSpan {
  uint32_t Edge;
  std::array<uint32_t, 2> Nodes;
};

struct SourceIndex {
  std::vector<BridgeSpan> Bridges;
  std::vector<uint32_t> Offsets;
  std::vector<uint32_t> Incidence;
  std::vector<uint8_t> Nodes;
};

BridgeInputError Error(BridgeInputErrorCode code,
                       std::optional<World::TransportEdgeId> edge = {},
                       std::optional<uint64_t> node = {}) {
  return {.Code = code, .Edge = edge, .Node = node};
}

bool Admit(size_t count, size_t stride, size_t &remaining) {
  if (count > remaining / stride) { return false; }
  remaining -= count * stride;
  return true;
}

bool Fits(const World::TransportTopology &graph, BridgeInputLimits limits) {
  const size_t edges = graph.Edges().size();
  const size_t nodes = graph.Nodes().size();
  if (edges > limits.SourceEdgesMost || edges > std::numeric_limits<uint32_t>::max() / 2 ||
      nodes >= std::numeric_limits<uint32_t>::max()) {
    return false;
  }
  size_t remaining = limits.ContainerCapacityMost;
  return Admit(edges, sizeof(BridgeSpan) + 4 * sizeof(uint32_t) + sizeof(uint8_t), remaining) &&
         Admit(nodes + 1, sizeof(uint32_t), remaining) &&
         Admit(
             nodes, sizeof(uint8_t) + sizeof(uint64_t) + sizeof(BridgeConditionRequest), remaining);
}

bool SamePhysical(const World::TransportEdgeId &a, const World::TransportEdgeId &b) {
  return a.PathId == b.PathId && a.SegmentOrdinal == b.SegmentOrdinal;
}

std::expected<uint32_t, BridgeInputError>
NodeIndex(const World::TransportTopology &graph, uint64_t id, BridgeInputWork &work) {
  ++work.NodeLookups;
  const auto *node = graph.FindNode(id);
  if (node == nullptr) {
    return std::unexpected(Error(BridgeInputErrorCode::InvalidSource, {}, id));
  }
  return static_cast<uint32_t>(node - graph.Nodes().data());
}

std::expected<bool, BridgeInputError>
CheckPhysicalAlias(std::span<const World::TransportEdge> edges, size_t at) {
  const auto &edge = edges[at];
  if (edge.Id.Direction != World::EdgeDirection::Forward &&
      edge.Id.Direction != World::EdgeDirection::Reverse) {
    return std::unexpected(Error(BridgeInputErrorCode::InvalidSource, edge.Id));
  }
  if (at == 0 || !SamePhysical(edge.Id, edges[at - 1].Id)) { return false; }
  const auto &previous = edges[at - 1];
  if (edge.Id == previous.Id || edge.FromNodeId != previous.ToNodeId ||
      edge.ToNodeId != previous.FromNodeId || edge.Bridge != previous.Bridge ||
      edge.Tunnel != previous.Tunnel || edge.Layer != previous.Layer) {
    return std::unexpected(Error(BridgeInputErrorCode::InvalidSource, edge.Id));
  }
  return true;
}

std::expected<SourceIndex, BridgeInputError> IndexSource(const World::TransportTopology &graph,
                                                         BridgeInputWork &work,
                                                         const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error(BridgeInputErrorCode::Canceled)); }
  SourceIndex index;
  const auto edges = graph.Edges();
  index.Bridges.reserve(edges.size());
  index.Offsets.assign(graph.Nodes().size() + 1, 0);
  index.Nodes.assign(graph.Nodes().size(), 0);
  for (size_t at = 0; at < edges.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error(BridgeInputErrorCode::Canceled)); }
    ++work.SourceEdges;
    const auto &edge = edges[at];
    const auto alias = CheckPhysicalAlias(edges, at);
    if (!alias) { return std::unexpected(alias.error()); }
    if (*alias) { continue; }
    const auto from = NodeIndex(graph, edge.FromNodeId, work);
    const auto to = NodeIndex(graph, edge.ToNodeId, work);
    if (!from) { return std::unexpected(from.error()); }
    if (!to) { return std::unexpected(to.error()); }
    if (*from == *to) {
      return std::unexpected(Error(BridgeInputErrorCode::InvalidSource, edge.Id));
    }
    ++work.PhysicalSegments;
    if (edge.Bridge) {
      index.Bridges.push_back({.Edge = static_cast<uint32_t>(at), .Nodes = {*from, *to}});
      ++index.Offsets[static_cast<size_t>(*from) + 1];
      ++index.Offsets[static_cast<size_t>(*to) + 1];
    } else if (!edge.Tunnel) {
      index.Nodes[*from] |= kGround;
      index.Nodes[*to] |= kGround;
    }
  }
  std::partial_sum(index.Offsets.begin(), index.Offsets.end(), index.Offsets.begin());
  const uint32_t count = index.Offsets.back();
  index.Incidence.resize(count);
  for (size_t at = 0; at < index.Bridges.size(); ++at) {
    for (const auto node : index.Bridges[at].Nodes) {
      index.Incidence[--index.Offsets[static_cast<size_t>(node) + 1]] = static_cast<uint32_t>(at);
    }
  }
  std::ranges::copy(std::span(index.Offsets).subspan(1), index.Offsets.begin());
  index.Offsets.back() = count;
  work.BridgeSegments = index.Bridges.size();
  work.AdjacencyEntries = count;
  return index;
}

size_t IndexCapacityBytes(const SourceIndex &index) {
  return index.Bridges.capacity() * sizeof(BridgeSpan) +
         index.Offsets.capacity() * sizeof(uint32_t) +
         index.Incidence.capacity() * sizeof(uint32_t) + index.Nodes.capacity();
}

std::expected<void, BridgeInputError> CheckSeeds(const World::TransportTopology &graph,
                                                 std::span<const World::TransportEdgeId> seeds,
                                                 BridgeInputWork &work,
                                                 const std::stop_token &stop) {
  for (const auto seed : seeds) {
    if (stop.stop_requested()) { return std::unexpected(Error(BridgeInputErrorCode::Canceled)); }
    ++work.SeedLookups;
    const auto *edge = graph.FindEdge(seed);
    if (edge == nullptr) { return std::unexpected(Error(BridgeInputErrorCode::MissingSeed, seed)); }
    if (!edge->Bridge) { return std::unexpected(Error(BridgeInputErrorCode::InvalidSeed, seed)); }
  }
  return {};
}

size_t SeedIndex(const World::TransportTopology &graph,
                 const SourceIndex &index,
                 World::TransportEdgeId seed,
                 BridgeInputWork &work) {
  ++work.SeedLookups;
  size_t at = static_cast<size_t>(graph.FindEdge(seed) - graph.Edges().data());
  if (at != 0 && SamePhysical(graph.Edges()[at - 1].Id, seed)) { --at; }
  const auto found = std::ranges::lower_bound(index.Bridges, at, {}, &BridgeSpan::Edge);
  return static_cast<size_t>(found - index.Bridges.begin());
}

struct Traversal {
  std::vector<uint8_t> Visited;
  std::vector<uint32_t> Queue;
};

void Schedule(uint32_t at,
              const SourceIndex &index,
              Traversal &state,
              BridgeInputRequirements &result) {
  if (state.Visited[at] != 0) { return; }
  state.Visited[at] = 1;
  state.Queue.push_back(at);
  result.BridgeEdges.push_back(index.Bridges[at].Edge);
}

bool VisitNode(uint32_t node,
               SourceIndex &index,
               Traversal &state,
               BridgeInputRequirements &result) {
  const uint64_t id = result.Source->Topology().Nodes()[node].SourceNodeId;
  auto &flags = index.Nodes[node];
  if ((flags & kGround) != 0) {
    if ((flags & kRecordedGround) == 0) {
      result.GroundNodes.push_back(id);
      flags |= kRecordedGround;
    }
    return true;
  }
  if ((flags & kExpanded) != 0) { return false; }
  flags |= kExpanded;
  const uint32_t first = index.Offsets[node];
  const uint32_t last = index.Offsets[static_cast<size_t>(node) + 1];
  if (last - first == 1) {
    result.Unqualified.push_back({.Node = id, .Reason = BridgeConditionReason::NoGroundConnection});
  }
  for (uint32_t at = first; at < last; ++at) {
    ++result.Work.AdjacencyVisits;
    Schedule(index.Incidence[at], index, state, result);
  }
  return false;
}

std::expected<void, BridgeInputError> Traverse(SourceIndex &index,
                                               std::span<const World::TransportEdgeId> seeds,
                                               BridgeInputRequirements &result,
                                               const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error(BridgeInputErrorCode::Canceled)); }
  Traversal state;
  state.Visited.assign(index.Bridges.size(), 0);
  state.Queue.reserve(index.Bridges.size());
  result.BridgeEdges.reserve(index.Bridges.size());
  const size_t nodes = result.Source->Topology().Nodes().size();
  result.GroundNodes.reserve(nodes);
  result.Unqualified.reserve(nodes);
  for (const auto seed : seeds) {
    const size_t first = SeedIndex(result.Source->Topology(), index, seed, result.Work);
    if (state.Visited[first] != 0) { continue; }
    state.Queue.clear();
    Schedule(static_cast<uint32_t>(first), index, state, result);
    uint64_t minimumNode = std::numeric_limits<uint64_t>::max();
    const size_t unqualifiedBefore = result.Unqualified.size();
    bool ground = false;
    for (size_t at = 0; at < state.Queue.size(); ++at) {
      if (stop.stop_requested()) { return std::unexpected(Error(BridgeInputErrorCode::Canceled)); }
      ++result.Work.BridgeVisits;
      for (const auto node : index.Bridges[state.Queue[at]].Nodes) {
        minimumNode = std::min(minimumNode, result.Source->Topology().Nodes()[node].SourceNodeId);
        ground = VisitNode(node, index, state, result) || ground;
      }
    }
    if (!ground && result.Unqualified.size() == unqualifiedBefore) {
      result.Unqualified.push_back(
          {.Node = minimumNode, .Reason = BridgeConditionReason::NoGroundAnchor});
    }
  }
  std::ranges::sort(result.BridgeEdges);
  std::ranges::sort(result.GroundNodes);
  std::ranges::sort(result.Unqualified, {}, &BridgeConditionRequest::Node);
  result.Work.PeakContainerCapacityBytes =
      IndexCapacityBytes(index) + state.Visited.capacity() +
      state.Queue.capacity() * sizeof(uint32_t) + result.BridgeEdges.capacity() * sizeof(uint32_t) +
      result.GroundNodes.capacity() * sizeof(uint64_t) +
      result.Unqualified.capacity() * sizeof(BridgeConditionRequest);
  return {};
}
}

std::expected<BridgeInputRequirements, BridgeInputError>
SelectBridgeInputRequirements(std::shared_ptr<const World::TransportNetworkSnapshot> source,
                              std::span<const World::TransportEdgeId> seeds,
                              BridgeInputLimits limits,
                              const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error(BridgeInputErrorCode::Canceled)); }
  BridgeInputRequirements result;
  result.Source = std::move(source);
  if (seeds.empty()) { return result; }
  if (!result.Source) { return std::unexpected(Error(BridgeInputErrorCode::MissingSource)); }
  const auto &graph = result.Source->Topology();
  const auto checked = CheckSeeds(graph, seeds, result.Work, stop);
  if (!checked) { return std::unexpected(checked.error()); }
  if (!Fits(graph, limits)) {
    return std::unexpected(Error(BridgeInputErrorCode::CapacityExceeded));
  }
  auto index = IndexSource(graph, result.Work, stop);
  if (!index) { return std::unexpected(index.error()); }
  const auto traversed = Traverse(*index, seeds, result, stop);
  if (!traversed) { return std::unexpected(traversed.error()); }
  if (result.Work.PeakContainerCapacityBytes > limits.ContainerCapacityMost) {
    return std::unexpected(Error(BridgeInputErrorCode::CapacityExceeded));
  }
  return result;
}
}
