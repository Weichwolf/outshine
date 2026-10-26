#include "TransportTopology.h"
#include "Units.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <utility>

namespace outshine::World {
namespace {
bool ValidNodes(std::span<const TransportNode> nodes) {
  for (size_t at = 0; at < nodes.size(); ++at) {
    const auto &node = nodes[at];
    if (!std::isfinite(node.LatitudeDeg) || !std::isfinite(node.LongitudeDeg) ||
        std::abs(node.LatitudeDeg) > kDegPerHalfTurn * 0.5 ||
        std::abs(node.LongitudeDeg) > kDegPerHalfTurn ||
        (at != 0 && nodes[at - 1].SourceNodeId >= node.SourceNodeId)) {
      return false;
    }
  }
  return true;
}

bool ValidEdges(const TransportTopology &graph) {
  constexpr uint32_t modes =
      static_cast<uint32_t>(TransportMode::Motor) | static_cast<uint32_t>(TransportMode::Walk) |
      static_cast<uint32_t>(TransportMode::Cycle) | static_cast<uint32_t>(TransportMode::Rail) |
      static_cast<uint32_t>(TransportMode::Water);
  const auto edges = graph.Edges();
  if (edges.size() > std::numeric_limits<uint32_t>::max()) { return false; }
  for (size_t at = 0; at < edges.size(); ++at) {
    const auto &edge = edges[at];
    const uint32_t edgeModes = edge.Modes;
    if ((edge.Id.Direction != EdgeDirection::Forward &&
         edge.Id.Direction != EdgeDirection::Reverse) ||
        (at != 0 && !(edges[at - 1].Id < edge.Id)) || (edgeModes & modes) != edgeModes ||
        edge.Facility > TransportFacility::Mixed || edge.Surface > TransportSurface::Water ||
        edge.Access > TransportAccess::Forbidden || !std::isfinite(edge.WidthM) ||
        edge.WidthM < 0 || graph.FindNode(edge.ToNodeId) == nullptr) {
      return false;
    }
  }
  return true;
}

std::expected<void, TransportTopologyError> ValidOutgoing(const TransportTopology &graph) {
  const auto outgoing = graph.Outgoing();
  const auto edges = graph.Edges();
  const auto nodes = graph.Nodes();
  if (outgoing.size() != edges.size()) {
    return std::unexpected(TransportTopologyError::InvalidOutgoing);
  }
  size_t nodeAt = 0;
  for (size_t at = 0; at < outgoing.size(); ++at) {
    const auto &entry = outgoing[at];
    if (entry.EdgeIndex >= edges.size()) {
      return std::unexpected(TransportTopologyError::InvalidOutgoing);
    }
    const auto from = edges[entry.EdgeIndex].FromNodeId;
    if (from != entry.NodeId) {
      return std::unexpected(graph.FindNode(from) == nullptr
                                 ? TransportTopologyError::InvalidEdges
                                 : TransportTopologyError::InvalidOutgoing);
    }
    while (nodeAt < nodes.size() && nodes[nodeAt].SourceNodeId < entry.NodeId) { ++nodeAt; }
    if (nodeAt == nodes.size() || nodes[nodeAt].SourceNodeId != entry.NodeId) {
      return std::unexpected(TransportTopologyError::InvalidEdges);
    }
    if (at != 0) {
      const auto &previous = outgoing[at - 1];
      if (previous.NodeId > entry.NodeId ||
          (previous.NodeId == entry.NodeId &&
           !(edges[previous.EdgeIndex].Id < edges[entry.EdgeIndex].Id))) {
        return std::unexpected(TransportTopologyError::InvalidOutgoing);
      }
    }
  }
  return {};
}
}

std::expected<void, TransportTopologyError>
ValidateTransportTopology(const TransportTopology &topology) {
  if (!ValidNodes(topology.Nodes())) {
    return std::unexpected(TransportTopologyError::InvalidNodes);
  }
  if (!ValidEdges(topology)) { return std::unexpected(TransportTopologyError::InvalidEdges); }
  return ValidOutgoing(topology);
}

TransportTopology::TransportTopology(PreparedTransportTopology data)
    : SourceIdentity_(std::move(data.SourceIdentity)),
      Nodes_(std::move(data.Nodes)),
      Edges_(std::move(data.Edges)),
      Outgoing_(std::move(data.Outgoing)),
      UnclassifiedPathCount_(data.UnclassifiedPathCount) {}

std::expected<TransportTopology, TransportTopologyError>
TransportTopology::RestorePrepared(PreparedTransportTopology data) {
  TransportTopology graph(std::move(data));
  const auto valid = ValidateTransportTopology(graph);
  if (!valid) { return std::unexpected(valid.error()); }
  return graph;
}

size_t TransportTopology::CapacityBytes() const noexcept {
  size_t bytes = sizeof(*this);
  const auto add = [&bytes](size_t count) {
    bytes += std::min(count, std::numeric_limits<size_t>::max() - bytes);
  };
  add(SourceIdentity_.DatasetId.capacity() + 1);
  add(SourceIdentity_.Revision.capacity() + 1);
  add(Nodes_.capacity() * sizeof(TransportNode));
  add(Edges_.capacity() * sizeof(TransportEdge));
  add(Outgoing_.capacity() * sizeof(OutgoingTransportEdge));
  return bytes;
}
}
