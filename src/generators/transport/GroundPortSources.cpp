#include "GroundPortSources.h"
#include "TransportNetworkSnapshot.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <numeric>
#include <span>
#include <stop_token>
#include <utility>

namespace outshine::Generators {
namespace {
constexpr double kSourceWidthMostM = 1000;
constexpr double kSourceGradientMost = 10;

bool SameSegment(World::TransportEdgeId a, World::TransportEdgeId b) noexcept {
  return a.PathId == b.PathId && a.SegmentOrdinal == b.SegmentOrdinal;
}

std::expected<size_t, GroundPortSourceError> Count(const World::TransportTopology &graph,
                                                   GroundPortSourceWork &work,
                                                   const std::stop_token &stop) {
  size_t count = 0;
  const auto edges = graph.Edges();
  for (size_t at = 0; at < edges.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(GroundPortSourceError::Canceled); }
    ++work.DirectedEdgesVisited;
    const auto &edge = edges[at];
    if (edge.FromNodeId == edge.ToNodeId || edge.FromNodeId == 0 || edge.ToNodeId == 0 ||
        edge.Id.PathId == 0 || edge.WidthM > kSourceWidthMostM || (edge.Bridge && edge.Tunnel)) {
      return std::unexpected(GroundPortSourceError::InvalidSource);
    }
    if (at == 0 || !SameSegment(edge.Id, edges[at - 1].Id)) {
      ++count;
      continue;
    }
    const auto &first = edges[at - 1];
    if (edge.FromNodeId != first.ToNodeId || edge.ToNodeId != first.FromNodeId ||
        edge.WidthM != first.WidthM || edge.Bridge != first.Bridge || edge.Tunnel != first.Tunnel ||
        edge.Layer != first.Layer) {
      return std::unexpected(GroundPortSourceError::InvalidSource);
    }
  }
  return count;
}

bool ValidProfiles(std::span<const GroundPortSourceProfile> profiles) noexcept {
  for (size_t at = 0; at < profiles.size(); ++at) {
    const auto &profile = profiles[at];
    if (profile.PathId == 0 || !std::isfinite(profile.MaximumGradient) ||
        profile.MaximumGradient <= 0 || profile.MaximumGradient > kSourceGradientMost ||
        (at != 0 && profiles[at - 1].PathId >= profile.PathId)) {
      return false;
    }
  }
  return true;
}

bool ValidCoverage(const GeographicBounds &box) noexcept {
  constexpr double kLatitudeMostDeg = kDegPerHalfTurn / 2;
  return std::isfinite(box.WestDeg) && std::isfinite(box.EastDeg) && std::isfinite(box.SouthDeg) &&
         std::isfinite(box.NorthDeg) && box.WestDeg >= -kDegPerHalfTurn &&
         box.EastDeg <= kDegPerHalfTurn && box.SouthDeg >= -kLatitudeMostDeg &&
         box.NorthDeg <= kLatitudeMostDeg && box.WestDeg < box.EastDeg &&
         box.SouthDeg < box.NorthDeg;
}
}

size_t GroundPortSources::CapacityBytes() const noexcept {
  return Segments_.capacity() * sizeof(Segment) + Offsets_.capacity() * sizeof(uint32_t) +
         Incidence_.capacity() * sizeof(uint32_t) + Unknown_.capacity();
}

std::expected<GroundPortSources, GroundPortSourceError>
GroundPortSources::Make(std::shared_ptr<const World::TransportNetworkSnapshot> source,
                        std::span<const GroundPortSourceProfile> profiles,
                        std::span<const uint64_t> unknownNodes,
                        GroundPortSourceLimits limits,
                        const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(GroundPortSourceError::Canceled); }
  if (!source) { return std::unexpected(GroundPortSourceError::MissingSource); }
  const auto &graph = source->Topology();
  const size_t nodes = graph.Nodes().size();
  if (nodes > limits.Nodes || graph.Edges().size() > limits.DirectedEdges ||
      source->Coverage().size() > limits.CoverageBoxes ||
      profiles.size() > limits.PhysicalSegments || unknownNodes.size() > nodes ||
      nodes >= std::numeric_limits<uint32_t>::max() ||
      graph.Edges().size() >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(GroundPortSourceError::CapacityExceeded);
  }
  if (nodes + 1 > limits.ContainerBytes / sizeof(uint32_t) ||
      nodes > limits.ContainerBytes - (nodes + 1) * sizeof(uint32_t)) {
    return std::unexpected(GroundPortSourceError::CapacityExceeded);
  }
  if (!World::ValidateTransportTopology(graph) ||
      (!graph.Nodes().empty() && graph.Nodes().front().SourceNodeId == 0) ||
      !std::ranges::all_of(source->Coverage(), ValidCoverage)) {
    return std::unexpected(GroundPortSourceError::InvalidSource);
  }
  if (!ValidProfiles(profiles)) { return std::unexpected(GroundPortSourceError::InvalidProfile); }
  GroundPortSources result;
  result.Work_.ValidatedNodes = nodes;
  result.Work_.ValidatedEdges = graph.Edges().size();
  result.Work_.ValidatedOutgoing = graph.Outgoing().size();
  const auto count = Count(graph, result.Work_, stop);
  if (!count) { return std::unexpected(count.error()); }
  if (*count > limits.PhysicalSegments || *count > std::numeric_limits<uint32_t>::max() / 2 ||
      nodes + 1 > limits.ContainerBytes / sizeof(uint32_t)) {
    return std::unexpected(GroundPortSourceError::CapacityExceeded);
  }
  const size_t fixed = (nodes + 1) * sizeof(uint32_t) + nodes;
  const size_t stride = sizeof(Segment) + 2 * sizeof(uint32_t);
  if (fixed > limits.ContainerBytes || *count > (limits.ContainerBytes - fixed) / stride) {
    return std::unexpected(GroundPortSourceError::CapacityExceeded);
  }
  result.Source_ = std::move(source);
  result.Segments_.reserve(*count);
  result.Offsets_.assign(nodes + 1, 0);
  result.Unknown_.assign(nodes, 0);
  result.Incidence_.resize(2 * *count);
  const auto indexed = result.Index(profiles, stop);
  if (!indexed) { return std::unexpected(indexed.error()); }
  const auto frontier = result.Frontier(unknownNodes, stop);
  if (!frontier) { return std::unexpected(frontier.error()); }
  result.Work_.ContainerCapacityBytes = result.CapacityBytes();
  if (result.Work_.ContainerCapacityBytes > limits.ContainerBytes) {
    return std::unexpected(GroundPortSourceError::CapacityExceeded);
  }
  return result;
}

std::expected<void, GroundPortSourceError>
GroundPortSources::Index(std::span<const GroundPortSourceProfile> profiles,
                         const std::stop_token &stop) {
  const auto &graph = Source_->Topology();
  const auto edges = graph.Edges();
  for (size_t at = 0; at < edges.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(GroundPortSourceError::Canceled); }
    ++Work_.DirectedEdgesVisited;
    const auto &edge = edges[at];
    if (at != 0 && SameSegment(edge.Id, edges[at - 1].Id)) { continue; }
    Work_.IndexNodeLookups += 2;
    const auto from = static_cast<uint32_t>(graph.FindNode(edge.FromNodeId) - graph.Nodes().data());
    const auto to = static_cast<uint32_t>(graph.FindNode(edge.ToNodeId) - graph.Nodes().data());
    ++Work_.ProfileLookups;
    const auto profile =
        std::ranges::lower_bound(profiles, edge.Id.PathId, {}, &GroundPortSourceProfile::PathId);
    const double gradient = profile != profiles.end() && profile->PathId == edge.Id.PathId
                                ? profile->MaximumGradient
                                : 0;
    std::array<uint32_t, 2> nodes{from, to};
    if (edge.Id.Direction == World::EdgeDirection::Reverse) { std::swap(nodes[0], nodes[1]); }
    Segments_.push_back(
        {.Edge = static_cast<uint32_t>(at), .Nodes = nodes, .MaximumGradient = gradient});
    ++Offsets_[static_cast<size_t>(from) + 1];
    ++Offsets_[static_cast<size_t>(to) + 1];
  }
  std::partial_sum(Offsets_.begin(), Offsets_.end(), Offsets_.begin());
  const uint32_t count = Offsets_.back();
  for (size_t at = Segments_.size(); at-- > 0;) {
    for (const auto node : Segments_[at].Nodes) {
      Incidence_[--Offsets_[static_cast<size_t>(node) + 1]] = static_cast<uint32_t>(at);
    }
  }
  std::ranges::copy(std::span(Offsets_).subspan(1), Offsets_.begin());
  Offsets_.back() = count;
  Work_.PhysicalSegments = Segments_.size();
  Work_.IncidenceEntries = count;
  return {};
}

std::expected<void, GroundPortSourceError>
GroundPortSources::Frontier(std::span<const uint64_t> unknownNodes, const std::stop_token &stop) {
  const auto &graph = Source_->Topology();
  for (size_t at = 0; at < unknownNodes.size(); ++at) {
    if (at != 0 && unknownNodes[at - 1] >= unknownNodes[at]) {
      return std::unexpected(GroundPortSourceError::InvalidFrontier);
    }
    ++Work_.FrontierNodeLookups;
    const auto *node = graph.FindNode(unknownNodes[at]);
    if (node == nullptr) { return std::unexpected(GroundPortSourceError::InvalidFrontier); }
    Unknown_[static_cast<size_t>(node - graph.Nodes().data())] = 1;
  }
  for (size_t at = 0; at < graph.Nodes().size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(GroundPortSourceError::Canceled); }
    const auto &node = graph.Nodes()[at];
    bool inside = false;
    for (const auto &box : Source_->Coverage()) {
      ++Work_.CoverageTests;
      if (box.WestDeg < node.LongitudeDeg && node.LongitudeDeg < box.EastDeg &&
          box.SouthDeg < node.LatitudeDeg && node.LatitudeDeg < box.NorthDeg) {
        inside = true;
        break;
      }
    }
    if (!inside) { Unknown_[at] = 1; }
  }
  return {};
}
}
