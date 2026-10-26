#include "GroundPortSources.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {

std::expected<void, GroundPortSourceError>
GroundPortSources::Expand(uint32_t node, Traversal &state, GroundPortChainLimits limits) const {
  if (state.Seen[node] != 0) { return {}; }
  state.Seen[node] = 1;
  ++state.Work.NodesVisited;
  state.Unknown = state.Unknown || Unknown_[node] != 0;
  for (uint32_t incidence = Offsets_[node]; incidence < Offsets_[static_cast<size_t>(node) + 1];
       ++incidence) {
    if (state.Work.IncidenceVisits >= limits.IncidenceVisits) {
      return std::unexpected(GroundPortSourceError::CapacityExceeded);
    }
    ++state.Work.IncidenceVisits;
    const auto next = Incidence_[incidence];
    if (state.Queued[next] != 0) { continue; }
    if (state.Queue.size() == limits.SelectedSegments) {
      return std::unexpected(GroundPortSourceError::CapacityExceeded);
    }
    state.Queued[next] = 1;
    state.Queue.push_back(next);
  }
  return {};
}

std::expected<GroundPortSourceChain, GroundPortSourceError> GroundPortSources::Chain(
    World::TransportEdgeId seed, GroundPortChainLimits limits, const std::stop_token &stop) const {
  if (stop.stop_requested()) { return std::unexpected(GroundPortSourceError::Canceled); }
  if (!Source_) { return std::unexpected(GroundPortSourceError::MissingSource); }
  if (seed.Direction != World::EdgeDirection::Forward &&
      seed.Direction != World::EdgeDirection::Reverse) {
    return std::unexpected(GroundPortSourceError::MissingSeed);
  }
  const auto &graph = Source_->Topology();
  const auto edges = graph.Edges();
  const auto selected = std::ranges::lower_bound(Segments_, seed, {}, [&](const Segment &span) {
    auto id = edges[span.Edge].Id;
    id.Direction = seed.Direction;
    return id;
  });
  if (selected == Segments_.end() || edges[selected->Edge].Id.PathId != seed.PathId ||
      edges[selected->Edge].Id.SegmentOrdinal != seed.SegmentOrdinal) {
    return std::unexpected(GroundPortSourceError::MissingSeed);
  }
  const size_t most = std::min(limits.SelectedSegments, Segments_.size());
  const size_t fixed = Segments_.size() + Unknown_.size();
  constexpr size_t kSegmentBytes = 3 * sizeof(uint32_t);
  constexpr size_t kExtraNodeBytes = sizeof(uint32_t);
  if (most == 0 || fixed > limits.ContainerBytes ||
      limits.ContainerBytes - fixed < kExtraNodeBytes ||
      most > (limits.ContainerBytes - fixed - kExtraNodeBytes) / kSegmentBytes) {
    return std::unexpected(GroundPortSourceError::CapacityExceeded);
  }
  Traversal state;
  state.Queued.assign(Segments_.size(), 0);
  state.Seen.assign(Unknown_.size(), 0);
  state.Queue.reserve(most);
  const auto first = static_cast<uint32_t>(selected - Segments_.begin());
  state.Queued[first] = 1;
  state.Queue.push_back(first);
  GroundPortSourceChain result{.Source = Source_, .Nodes = {}, .SourceEdges = {}, .Work = {}};
  for (size_t at = 0; at < state.Queue.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(GroundPortSourceError::Canceled); }
    ++state.Work.SelectedSegments;
    for (const auto node : Segments_[state.Queue[at]].Nodes) {
      const auto expanded = Expand(node, state, limits);
      if (!expanded) { return std::unexpected(expanded.error()); }
    }
  }
  if (state.Unknown) { return std::unexpected(GroundPortSourceError::UnknownFrontier); }
  std::ranges::sort(state.Queue);
  result.Nodes.reserve(state.Queue.size() + 1);
  result.SourceEdges.reserve(state.Queue.size());
  result.Work = state.Work;
  const auto ordered = Order(state.Queue, result);
  if (!ordered) { return std::unexpected(ordered.error()); }
  result.Work.ContainerCapacityBytes =
      state.Queued.capacity() + state.Seen.capacity() + state.Queue.capacity() * sizeof(uint32_t) +
      result.Nodes.capacity() * sizeof(uint32_t) + result.SourceEdges.capacity() * sizeof(uint32_t);
  if (result.Work.ContainerCapacityBytes > limits.ContainerBytes) {
    return std::unexpected(GroundPortSourceError::CapacityExceeded);
  }
  return result;
}

std::expected<uint32_t, GroundPortSourceError>
GroundPortSources::Qualify(std::span<const uint32_t> component,
                           GroundPortSourceChain &result) const {
  const auto edges = Source_->Topology().Edges();
  uint32_t first = std::numeric_limits<uint32_t>::max();
  size_t ends = 0;
  result.WidthM = edges[Segments_[component.front()].Edge].WidthM;
  result.MaximumGradient = Segments_[component.front()].MaximumGradient;
  for (const auto at : component) {
    ++result.Work.ProfileSegmentsChecked;
    const auto &segment = Segments_[at];
    const auto &edge = edges[segment.Edge];
    if (edge.Bridge) { return std::unexpected(GroundPortSourceError::BridgeRecipeRequired); }
    if (edge.Tunnel) { return std::unexpected(GroundPortSourceError::TunnelRecipeRequired); }
    if (edge.Layer != 0) { return std::unexpected(GroundPortSourceError::LayerRecipeRequired); }
    if (edge.WidthM <= 0 || segment.MaximumGradient <= 0) {
      return std::unexpected(GroundPortSourceError::MissingProfile);
    }
    if (edge.WidthM != result.WidthM || segment.MaximumGradient != result.MaximumGradient) {
      return std::unexpected(GroundPortSourceError::MixedProfiles);
    }
    for (const auto node : segment.Nodes) {
      const uint32_t degree = Offsets_[static_cast<size_t>(node) + 1] - Offsets_[node];
      if (degree > 2) { return std::unexpected(GroundPortSourceError::BranchedOrClosed); }
      if (degree == 1) {
        ++ends;
        first = std::min(first, node);
      }
    }
  }
  if (ends != 2) { return std::unexpected(GroundPortSourceError::BranchedOrClosed); }
  return first;
}

std::expected<void, GroundPortSourceError>
GroundPortSources::Order(std::span<const uint32_t> component, GroundPortSourceChain &result) const {
  const auto first = Qualify(component, result);
  if (!first) { return std::unexpected(first.error()); }
  result.Nodes.push_back(*first);
  uint32_t previous = std::numeric_limits<uint32_t>::max();
  while (result.SourceEdges.size() < component.size()) {
    const uint32_t node = result.Nodes.back();
    const uint32_t begin = Offsets_[node];
    const uint32_t end = Offsets_[static_cast<size_t>(node) + 1];
    uint32_t next = Incidence_[begin];
    if (next == previous) {
      if (end - begin != 2) { break; }
      next = Incidence_[begin + 1];
    }
    const auto &segment = Segments_[next];
    ++result.Work.OrderedSegments;
    result.SourceEdges.push_back(segment.Edge);
    result.Nodes.push_back(segment.Nodes[0] == node ? segment.Nodes[1] : segment.Nodes[0]);
    previous = next;
  }
  if (result.SourceEdges.size() != component.size() ||
      Offsets_[static_cast<size_t>(result.Nodes.back()) + 1] - Offsets_[result.Nodes.back()] != 1) {
    return std::unexpected(GroundPortSourceError::AmbiguousParallel);
  }
  return {};
}
}
