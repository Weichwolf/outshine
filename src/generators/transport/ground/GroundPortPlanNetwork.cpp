#include "GroundPortPlanNetwork.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
std::expected<TransportPlanNetwork, TransportPlanNetworkError> ProjectGroundPortFieldNetwork(
    const GroundPortField &field, TransportPlanNetworkLimits limits, const std::stop_token &stop) {
  using Error = TransportPlanNetworkError;
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  const auto &source = field.Source();
  if (!source.Source) { return std::unexpected(Error::InvalidSource); }
  const auto &graph = source.Source->Topology();
  if (source.SourceEdges.size() > limits.DirectedEdges / 2) {
    return std::unexpected(Error::CapacityExceeded);
  }
  std::vector<World::TransportEdgeId> selected;
  selected.reserve(source.SourceEdges.size() * 2);
  const auto append = [&](World::TransportEdgeId id) {
    selected.push_back(id);
    id.Direction = id.Direction == World::EdgeDirection::Forward ? World::EdgeDirection::Reverse
                                                                 : World::EdgeDirection::Forward;
    if (graph.FindEdge(id) != nullptr) { selected.push_back(id); }
  };
  for (const auto index : source.SourceEdges) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (index >= graph.Edges().size()) { return std::unexpected(Error::InvalidSource); }
    append(graph.Edges()[index].Id);
  }
  std::ranges::sort(selected);
  selected.erase(std::ranges::unique(selected).begin(), selected.end());
  auto result = ProjectTransportPlanNetwork(*source.Source, selected, limits, stop);
  if (!result) { return std::unexpected(result.error()); }
  result->Work.EdgeLookups += source.SourceEdges.size();
  result->Work.ScratchCapacityBytes += selected.capacity() * sizeof(World::TransportEdgeId);
  return result;
}
}
