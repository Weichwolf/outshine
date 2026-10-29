#include "GroundPortPlanNetwork.h"
#include "TransportPlanNetworkBinding.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <utility>
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

GroundPortPlanNetwork::GroundPortPlanNetwork(const GroundPortField &field,
                                             TransportPlanNetwork projection,
                                             std::array<uint8_t, 32> binding)
    : Source_(field.Source().Source),
      SourceEdges_(field.Source().SourceEdges),
      Projection_(std::move(projection)),
      Binding_(binding) {}

std::expected<GroundPortPlanNetwork, GroundPortPlanNetwork::Error>
GroundPortPlanNetwork::Prepare(const GroundPortField &field,
                               TransportPlanNetworkLimits limits,
                               size_t bindingBytesMost,
                               const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(TransportPlanNetworkError::Canceled); }
  if (limits.OutputCapacityBytes < sizeof(GroundPortPlanNetwork) ||
      field.Source().SourceEdges.size() >
          (limits.OutputCapacityBytes - sizeof(GroundPortPlanNetwork)) / sizeof(uint32_t)) {
    return std::unexpected(TransportPlanNetworkError::CapacityExceeded);
  }
  const size_t bytesMost = limits.OutputCapacityBytes;
  limits.OutputCapacityBytes -=
      sizeof(GroundPortPlanNetwork) + field.Source().SourceEdges.size() * sizeof(uint32_t);
  auto projection = ProjectGroundPortFieldNetwork(field, limits, stop);
  if (!projection) { return std::unexpected(projection.error()); }
  const auto binding = TransportPlanNetworkBinding(projection->Network, bindingBytesMost);
  if (!binding) { return std::unexpected(binding.error()); }
  GroundPortPlanNetwork result(field, std::move(*projection), *binding);
  if (result.CapacityBytes() > bytesMost) {
    return std::unexpected(TransportPlanNetworkError::CapacityExceeded);
  }
  if (stop.stop_requested()) { return std::unexpected(TransportPlanNetworkError::Canceled); }
  return result;
}

bool GroundPortPlanNetwork::Matches(const GroundPortField &field) const noexcept {
  const auto source = Source_.lock();
  return source && source == field.Source().Source && SourceEdges_ == field.Source().SourceEdges;
}

size_t GroundPortPlanNetwork::CapacityBytes() const noexcept {
  return sizeof(*this) + SourceEdges_.capacity() * sizeof(uint32_t) +
         Projection_.Work.OutputCapacityBytes;
}
}
