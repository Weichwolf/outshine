#include "TransportNetworkSnapshot.h"
#include "Units.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>

namespace outshine::World {
namespace {
bool ValidCoverage(const GeographicBounds &bounds) {
  return std::isfinite(bounds.WestDeg) && std::isfinite(bounds.EastDeg) &&
         std::isfinite(bounds.SouthDeg) && std::isfinite(bounds.NorthDeg) &&
         std::abs(bounds.WestDeg) <= kDegPerHalfTurn &&
         std::abs(bounds.EastDeg) <= kDegPerHalfTurn &&
         std::abs(bounds.SouthDeg) <= kDegPerHalfTurn * 0.5 &&
         std::abs(bounds.NorthDeg) <= kDegPerHalfTurn * 0.5 && bounds.SouthDeg <= bounds.NorthDeg;
}

bool ValidRoute(const NamedCircuitRoute &route, const TransportTopology &graph) {
  const auto &circuit = route.Circuit;
  if (circuit.SourceIdentity != graph.SourceIdentity()) { return false; }
  uint64_t node = circuit.StartNodeId;
  for (const auto id : circuit.EdgeIds) {
    const auto *edge = graph.FindEdge(id);
    if (edge == nullptr || edge->FromNodeId != node) { return false; }
    node = edge->ToNodeId;
  }
  return node == circuit.StartNodeId &&
         (circuit.EdgeIds.empty() || graph.FindNode(circuit.StartNodeId) != nullptr);
}
}

std::expected<void, TransportNetworkError>
ValidateTransportNetwork(const TransportNetworkSnapshot &network) {
  if (!ValidateTransportTopology(network.Topology())) {
    return std::unexpected(TransportNetworkError::InvalidTopology);
  }
  if (!ValidateTransportProvenance(network.Topology(), network.Provenance())) {
    return std::unexpected(TransportNetworkError::InvalidProvenance);
  }
  if (!std::ranges::all_of(network.Coverage(), ValidCoverage)) {
    return std::unexpected(TransportNetworkError::InvalidCoverage);
  }
  if (!std::ranges::all_of(network.Routes(), [&network](const auto &route) {
        return ValidRoute(route, network.Topology());
      })) {
    return std::unexpected(TransportNetworkError::InvalidRoutes);
  }
  return {};
}

size_t TransportNetworkSnapshot::CapacityBytes() const noexcept {
  size_t bytes = sizeof(*this);
  const auto add = [&bytes](size_t count) {
    bytes += std::min(count, std::numeric_limits<size_t>::max() - bytes);
  };
  add(Transport_.Graph.CapacityBytes() - sizeof(TransportTopology));
  add(Coverage_.capacity() * sizeof(GeographicBounds));
  add(Transport_.Routes.capacity() * sizeof(NamedCircuitRoute));
  add(Provenance_.Sources.capacity() * sizeof(TransportSource));
  add(Provenance_.Paths.capacity() * sizeof(TransportPathSource));
  add(Provenance_.References.capacity() * sizeof(TransportSourceReference));
  for (const auto &source : Provenance_.Sources) {
    add(source.Identity.DatasetId.capacity() + 1);
    add(source.Identity.Revision.capacity() + 1);
    add(source.Item.capacity() + 1);
  }
  for (const auto &route : Transport_.Routes) {
    add(route.Id.capacity() + 1);
    add(route.Circuit.SourceIdentity.DatasetId.capacity() + 1);
    add(route.Circuit.SourceIdentity.Revision.capacity() + 1);
    add(route.Circuit.EdgeIds.capacity() * sizeof(TransportEdgeId));
  }
  return bytes;
}
}
