#include "TransportPlanNetwork.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Error = TransportPlanNetworkError;
template <class T> using Checked = std::expected<T, Error>;

template <class T> void SortUnique(std::vector<T> &values) {
  std::ranges::sort(values);
  values.erase(std::ranges::unique(values).begin(), values.end());
}

bool Consume(size_t count, size_t &remaining) {
  if (count > remaining) { return false; }
  remaining -= count;
  return true;
}

Checked<std::vector<World::TransportEdge>>
SelectEdges(const World::TransportTopology &source,
            std::span<const World::TransportEdgeId> selected,
            TransportPlanNetworkWork &work,
            const std::stop_token &stop) {
  std::vector<World::TransportEdge> edges;
  edges.reserve(selected.size());
  for (const auto id : selected) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    ++work.EdgeLookups;
    const auto *edge = source.FindEdge(id);
    if (edge == nullptr) { return std::unexpected(Error::MissingEdge); }
    edges.push_back(*edge);
  }
  std::ranges::sort(edges, {}, &World::TransportEdge::Id);
  edges.erase(std::ranges::unique(edges, {}, &World::TransportEdge::Id).begin(), edges.end());
  work.CopiedEdgeBytes = selected.size() * sizeof(World::TransportEdge);
  return edges;
}

Checked<std::vector<World::TransportNode>> SelectNodes(const World::TransportTopology &source,
                                                       std::span<const World::TransportEdge> edges,
                                                       TransportPlanNetworkWork &work,
                                                       const std::stop_token &stop) {
  std::vector<uint64_t> ids;
  ids.reserve(edges.size() * 2);
  for (const auto &edge : edges) {
    ids.push_back(edge.FromNodeId);
    ids.push_back(edge.ToNodeId);
  }
  SortUnique(ids);
  work.ScratchCapacityBytes = ids.capacity() * sizeof(uint64_t);
  std::vector<World::TransportNode> nodes;
  nodes.reserve(ids.size());
  for (const auto id : ids) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    ++work.NodeLookups;
    const auto *node = source.FindNode(id);
    if (node == nullptr) { return std::unexpected(Error::InvalidSource); }
    nodes.push_back(*node);
  }
  return nodes;
}

Checked<void> RemapSources(World::TransportProvenance &result,
                           const World::TransportProvenance &source,
                           std::span<const uint32_t> sources,
                           size_t &textRemaining) {
  result.Sources.reserve(sources.size());
  for (const auto index : sources) {
    const auto &item = source.Sources[index];
    if (!Consume(item.Identity.DatasetId.size(), textRemaining) ||
        !Consume(item.Identity.Revision.size(), textRemaining) ||
        !Consume(item.Item.size(), textRemaining)) {
      return std::unexpected(Error::CapacityExceeded);
    }
    result.Sources.push_back(item);
  }
  for (auto &reference : result.References) {
    reference.Source = static_cast<uint32_t>(std::ranges::lower_bound(sources, reference.Source) -
                                             sources.begin());
  }
  return {};
}

Checked<World::TransportProvenance> SelectProvenance(const World::TransportProvenance &source,
                                                     std::span<const World::TransportEdge> edges,
                                                     TransportPlanNetworkLimits limits,
                                                     TransportPlanNetworkWork &work,
                                                     size_t &textRemaining,
                                                     const std::stop_token &stop) {
  World::TransportProvenance result;
  result.IdentitySpace = source.IdentitySpace;
  if (source.Sources.empty()) {
    if (!source.Paths.empty() || !source.References.empty()) {
      return std::unexpected(Error::InvalidSource);
    }
    return result;
  }
  result.Paths.reserve(edges.size());
  size_t references = 0;
  for (const auto &edge : edges) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (!result.Paths.empty() && result.Paths.back().PathId == edge.Id.PathId) { continue; }
    ++work.PathLookups;
    const auto path = std::ranges::lower_bound(
        source.Paths, edge.Id.PathId, {}, &World::TransportPathSource::PathId);
    if (path == source.Paths.end() || path->PathId != edge.Id.PathId ||
        path->FirstReference > source.References.size() || path->ReferenceCount == 0 ||
        path->ReferenceCount > source.References.size() - path->FirstReference) {
      return std::unexpected(Error::InvalidSource);
    }
    if (path->ReferenceCount > limits.References - references) {
      return std::unexpected(Error::CapacityExceeded);
    }
    result.Paths.push_back(*path);
    references += path->ReferenceCount;
  }
  result.References.reserve(references);
  std::vector<uint32_t> sources;
  sources.reserve(references);
  for (auto &path : result.Paths) {
    const auto first = path.FirstReference;
    path.FirstReference = static_cast<uint32_t>(result.References.size());
    for (size_t at = 0; at < path.ReferenceCount; ++at) {
      if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
      ++work.ReferenceVisits;
      const auto &reference = source.References[first + at];
      if (reference.Source >= source.Sources.size()) {
        return std::unexpected(Error::InvalidSource);
      }
      result.References.push_back(reference);
      sources.push_back(reference.Source);
    }
  }
  SortUnique(sources);
  work.ScratchCapacityBytes =
      std::max(work.ScratchCapacityBytes, sources.capacity() * sizeof(uint32_t));
  const auto remapped = RemapSources(result, source, sources, textRemaining);
  if (!remapped) { return std::unexpected(remapped.error()); }
  return result;
}

Checked<std::vector<World::NamedCircuitRoute>>
SelectRoutes(const World::TransportNetworkSnapshot &source,
             const World::TransportTopology &graph,
             TransportPlanNetworkLimits limits,
             TransportPlanNetwork &result,
             size_t &textRemaining,
             const std::stop_token &stop) {
  std::vector<World::NamedCircuitRoute> routes;
  size_t routeEdges = limits.RouteEdges;
  for (const auto &route : source.Routes()) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (!Consume(route.Id.size(), textRemaining) ||
        !Consume(route.Circuit.EdgeIds.size(), routeEdges)) {
      return std::unexpected(Error::CapacityExceeded);
    }
    bool complete = graph.FindNode(route.Circuit.StartNodeId) != nullptr;
    for (const auto edge : route.Circuit.EdgeIds) {
      if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
      ++result.Work.RouteEdgeVisits;
      complete = (graph.FindEdge(edge) != nullptr) && complete;
    }
    if (complete) {
      if (!Consume(route.Circuit.SourceIdentity.DatasetId.size(), textRemaining) ||
          !Consume(route.Circuit.SourceIdentity.Revision.size(), textRemaining)) {
        return std::unexpected(Error::CapacityExceeded);
      }
      routes.push_back(route);
    } else {
      result.OmittedRoutes.push_back(
          {.Id = route.Id, .SourceRouteId = route.Circuit.SourceRouteId});
    }
  }
  return routes;
}
}

std::expected<TransportPlanNetwork, TransportPlanNetworkError>
ProjectTransportPlanNetwork(const World::TransportNetworkSnapshot &source,
                            std::span<const World::TransportEdgeId> directedEdges,
                            TransportPlanNetworkLimits limits,
                            const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (directedEdges.empty()) { return std::unexpected(Error::EmptySelection); }
  if (directedEdges.size() > limits.DirectedEdges || source.RouteCount() > limits.Routes ||
      directedEdges.size() > std::numeric_limits<uint32_t>::max() / 2 ||
      limits.References > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  size_t textRemaining = limits.TextBytes;
  if (source.SourceIdentity().DatasetId.empty() || source.SourceIdentity().Revision.empty()) {
    return std::unexpected(Error::InvalidSource);
  }
  if (!Consume(source.SourceIdentity().DatasetId.size(), textRemaining) ||
      !Consume(source.SourceIdentity().Revision.size(), textRemaining)) {
    return std::unexpected(Error::CapacityExceeded);
  }
  TransportPlanNetwork result;
  auto edges = SelectEdges(source.Topology(), directedEdges, result.Work, stop);
  if (!edges) { return std::unexpected(edges.error()); }
  auto nodes = SelectNodes(source.Topology(), *edges, result.Work, stop);
  if (!nodes) { return std::unexpected(nodes.error()); }
  auto provenance =
      SelectProvenance(source.Provenance(), *edges, limits, result.Work, textRemaining, stop);
  if (!provenance) { return std::unexpected(provenance.error()); }
  World::TransportTopology graph(source.SourceIdentity(),
                                 std::move(*nodes),
                                 std::move(*edges),
                                 source.Topology().UnclassifiedPathCount());
  auto routes = SelectRoutes(source, graph, limits, result, textRemaining, stop);
  if (!routes) { return std::unexpected(routes.error()); }
  result.Network = std::make_shared<const World::TransportNetworkSnapshot>(
      World::ResolvedTransport{.Graph = std::move(graph), .Routes = std::move(*routes)},
      std::vector<GeographicBounds>{},
      World::TransportLoadMetrics{},
      std::move(*provenance));
  if (!World::ValidateTransportNetwork(*result.Network)) {
    return std::unexpected(Error::InvalidSource);
  }
  size_t outputRemaining = limits.OutputCapacityBytes;
  if (!Consume(result.Network->CapacityBytes(), outputRemaining) ||
      result.OmittedRoutes.capacity() > outputRemaining / sizeof(TransportPlanRouteOmission)) {
    return std::unexpected(Error::CapacityExceeded);
  }
  outputRemaining -= result.OmittedRoutes.capacity() * sizeof(TransportPlanRouteOmission);
  for (const auto &route : result.OmittedRoutes) {
    if (!Consume(route.Id.capacity() + 1, outputRemaining)) {
      return std::unexpected(Error::CapacityExceeded);
    }
  }
  result.Work.OutputCapacityBytes = limits.OutputCapacityBytes - outputRemaining;
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  return result;
}
}
