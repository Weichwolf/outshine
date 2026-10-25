#include "TransportPlanFormat.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::TransportFormat {
namespace {
template <class Archive, class Node> bool NodeFields(Archive &archive, Node &node) {
  return archive(node.SourceNodeId, node.LatitudeDeg, node.LongitudeDeg);
}

template <class Archive, class Id> bool EdgeIdFields(Archive &archive, Id &id) {
  return archive(id.PathId, id.SegmentOrdinal, id.Direction);
}

template <class Archive, class Edge> bool EdgeFields(Archive &archive, Edge &edge) {
  return EdgeIdFields(archive, edge.Id) && archive(edge.FromNodeId,
                                                   edge.ToNodeId,
                                                   edge.Modes,
                                                   edge.Facility,
                                                   edge.Surface,
                                                   edge.WidthM,
                                                   edge.LaneCount,
                                                   edge.Layer,
                                                   edge.Bridge,
                                                   edge.Tunnel,
                                                   edge.Access);
}

template <class Archive, class Bounds> bool CoverageFields(Archive &archive, Bounds &bounds) {
  return archive(bounds.WestDeg, bounds.SouthDeg, bounds.EastDeg, bounds.NorthDeg);
}

bool RouteFields(BinaryValueWriter &out, const World::NamedCircuitRoute &route) {
  const auto &circuit = route.Circuit;
  return out.Text(route.Id) && out.Text(circuit.SourceIdentity.DatasetId) &&
         out.Text(circuit.SourceIdentity.Revision) &&
         out(circuit.SourceRouteId, circuit.StartNodeId) &&
         List(out, std::span(circuit.EdgeIds), [&](const auto &id) {
           return EdgeIdFields(out, id);
         });
}

bool RouteFields(Reader &in, World::NamedCircuitRoute &route) {
  auto &circuit = route.Circuit;
  return in.Text(route.Id) && in.Text(circuit.SourceIdentity.DatasetId) &&
         in.Text(circuit.SourceIdentity.Revision) &&
         in(circuit.SourceRouteId, circuit.StartNodeId) &&
         in.List(
             circuit.EdgeIds, 2 * sizeof(uint64_t), [&](auto &id) { return EdgeIdFields(in, id); });
}

bool TopologyFields(BinaryValueWriter &out, const World::TransportTopology &graph) {
  return out.Text(graph.SourceIdentity().DatasetId) && out.Text(graph.SourceIdentity().Revision) &&
         out(static_cast<uint64_t>(graph.UnclassifiedPathCount())) &&
         List(out, graph.Nodes(), [&](const auto &node) { return NodeFields(out, node); }) &&
         List(out, graph.Edges(), [&](const auto &edge) { return EdgeFields(out, edge); }) &&
         List(out, graph.Outgoing(), [&](const auto &edge) {
           return out(edge.NodeId, edge.EdgeIndex);
         });
}

bool TopologyFields(Reader &in, World::PreparedTransportTopology &data) {
  uint64_t count = 0;
  constexpr size_t nodeBytes = 3 * sizeof(uint64_t);
  constexpr size_t edgeBytes = 60;
  constexpr size_t outgoingBytes = sizeof(uint64_t) + sizeof(uint32_t);
  if (!in.Text(data.SourceIdentity.DatasetId) || !in.Text(data.SourceIdentity.Revision) ||
      !in(count) || count > std::numeric_limits<size_t>::max() ||
      !in.List(data.Nodes, nodeBytes, [&](auto &node) { return NodeFields(in, node); }) ||
      !in.List(data.Edges, edgeBytes, [&](auto &edge) { return EdgeFields(in, edge); }) ||
      !in.List(data.Outgoing, outgoingBytes, [&](auto &edge) {
        return in(edge.NodeId, edge.EdgeIndex);
      })) {
    return false;
  }
  data.UnclassifiedPathCount = static_cast<size_t>(count);
  return true;
}
}

bool Network(BinaryValueWriter &out,
             const std::shared_ptr<const World::TransportNetworkSnapshot> &network) {
  if (!out(network != nullptr)) { return false; }
  if (!network) { return true; }
  return TopologyFields(out, network->Topology()) &&
         List(out,
              network->Coverage(),
              [&](const auto &bounds) { return CoverageFields(out, bounds); }) &&
         List(out, network->Routes(), [&](const auto &route) { return RouteFields(out, route); }) &&
         Provenance(out, network->Provenance());
}

bool Network(Reader &in, std::shared_ptr<const World::TransportNetworkSnapshot> &network) {
  bool present = false;
  if (!in(present)) { return false; }
  if (!present) {
    network.reset();
    return true;
  }
  if (!in.Charge(sizeof(World::TransportNetworkSnapshot))) { return false; }
  World::PreparedTransportTopology data;
  std::vector<GeographicBounds> coverage;
  std::vector<World::NamedCircuitRoute> routes;
  World::TransportProvenance provenance;
  if (!TopologyFields(in, data) ||
      !in.List(
          coverage, 4 * sizeof(double), [&](auto &bounds) { return CoverageFields(in, bounds); }) ||
      !in.List(routes, 4 * sizeof(uint64_t), [&](auto &route) { return RouteFields(in, route); }) ||
      !Provenance(in, provenance)) {
    return false;
  }
  auto graph = World::TransportTopology::RestorePrepared(std::move(data));
  if (!graph || std::ranges::any_of(routes, [&graph](const auto &route) {
        return route.Circuit.SourceIdentity != graph->SourceIdentity();
      })) {
    return false;
  }
  auto restored = std::make_shared<const World::TransportNetworkSnapshot>(
      World::ResolvedTransport{.Graph = std::move(*graph), .Routes = std::move(routes)},
      std::move(coverage),
      World::TransportLoadMetrics{},
      std::move(provenance));
  network = std::move(restored);
  return true;
}
}
