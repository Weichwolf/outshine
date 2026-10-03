#include "OsmTransportBuilder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace outshine::Generators::Osm {

namespace {

bool IsCircuit(const Relation &relation) {
  size_t kinds = 0;
  for (const Tag &tag : relation.Tags) {
    if (tag.Key != "type") { continue; }
    ++kinds;
    if (tag.Value != "circuit") { return false; }
  }
  return kinds == 1;
}

std::expected<const World::TransportEdge *, CircuitError>
ResolveSegment(const World::TransportTopology &graph, const Way &way, size_t segment) {
  const World::TransportEdgeId forwardId{.PathId = way.Id,
                                         .SegmentOrdinal = static_cast<uint32_t>(segment),
                                         .Direction = World::EdgeDirection::Forward};
  const World::TransportEdgeId reverseId{.PathId = way.Id,
                                         .SegmentOrdinal = static_cast<uint32_t>(segment),
                                         .Direction = World::EdgeDirection::Reverse};
  const World::TransportEdge *forward = graph.FindEdge(forwardId);
  const World::TransportEdge *reverse = graph.FindEdge(reverseId);
  if (forward != nullptr && reverse != nullptr) {
    return std::unexpected(
        CircuitError{.Code = CircuitErrorCode::AmbiguousDirection, .SourceId = way.Id});
  }
  const World::TransportEdge *edge = forward != nullptr ? forward : reverse;
  if (edge == nullptr) {
    return std::unexpected(CircuitError{.Code = CircuitErrorCode::MissingEdge, .SourceId = way.Id});
  }
  const uint64_t expectedFrom =
      forward != nullptr ? way.NodeIds[segment] : way.NodeIds[segment + 1];
  const uint64_t expectedTo = forward != nullptr ? way.NodeIds[segment + 1] : way.NodeIds[segment];
  if (edge->FromNodeId != expectedFrom || edge->ToNodeId != expectedTo) {
    return std::unexpected(CircuitError{.Code = CircuitErrorCode::MissingEdge, .SourceId = way.Id});
  }
  if (edge->Modes == 0 || edge->Access == World::TransportAccess::Forbidden) {
    return std::unexpected(
        CircuitError{.Code = CircuitErrorCode::UnusableEdge, .SourceId = way.Id});
  }
  return edge;
}

std::expected<std::vector<const World::TransportEdge *>, CircuitError>
SelectEdges(const World::TransportTopology &graph,
            const ElementSet &source,
            const Relation &relation,
            std::string_view memberRole,
            size_t maxEdges) {
  std::vector<const World::TransportEdge *> selected;
  for (const RelationMember &member : relation.Members) {
    if (member.Role != memberRole) { continue; }
    if (member.Kind != ElementKind::Way) {
      return std::unexpected(
          CircuitError{.Code = CircuitErrorCode::InvalidMember, .SourceId = member.Id});
    }
    const Way *way = source.FindWay(member.Id);
    if (way == nullptr) {
      return std::unexpected(
          CircuitError{.Code = CircuitErrorCode::MissingWay, .SourceId = member.Id});
    }
    if (way->NodeIds.size() < 2) {
      return std::unexpected(
          CircuitError{.Code = CircuitErrorCode::InvalidMember, .SourceId = member.Id});
    }
    for (size_t segment = 0; segment + 1 < way->NodeIds.size(); ++segment) {
      if (selected.size() == maxEdges) {
        return std::unexpected(
            CircuitError{.Code = CircuitErrorCode::TooManyEdges, .SourceId = relation.Id});
      }
      const auto edge = ResolveSegment(graph, *way, segment);
      if (!edge) { return std::unexpected(edge.error()); }
      selected.push_back(*edge);
    }
  }
  if (selected.empty()) {
    return std::unexpected(
        CircuitError{.Code = CircuitErrorCode::EmptyRoute, .SourceId = relation.Id});
  }
  return selected;
}

}

std::expected<World::CircuitRoute, CircuitError>
TransportBuilder::ResolveCircuit(const World::TransportTopology &graph,
                                 const ElementSet &source,
                                 uint64_t relationId,
                                 std::string_view memberRole,
                                 size_t maxEdges) {
  if (source.SourceIdentity() != graph.SourceIdentity()) {
    return std::unexpected(
        CircuitError{.Code = CircuitErrorCode::SourceMismatch, .SourceId = relationId});
  }
  const Relation *relation = source.FindRelation(relationId);
  if (relation == nullptr) {
    return std::unexpected(
        CircuitError{.Code = CircuitErrorCode::MissingRelation, .SourceId = relationId});
  }
  if (!IsCircuit(*relation)) {
    return std::unexpected(
        CircuitError{.Code = CircuitErrorCode::NotCircuit, .SourceId = relationId});
  }
  const auto selected = SelectEdges(graph, source, *relation, memberRole, maxEdges);
  if (!selected) { return std::unexpected(selected.error()); }
  std::unordered_map<uint64_t, const World::TransportEdge *> outgoing;
  outgoing.reserve(selected->size());
  for (const World::TransportEdge *edge : *selected) {
    if (!outgoing.emplace(edge->FromNodeId, edge).second) {
      return std::unexpected(
          CircuitError{.Code = CircuitErrorCode::AmbiguousDirection, .SourceId = edge->FromNodeId});
    }
  }
  const World::TransportEdge *first =
      *std::min_element(selected->begin(),
                        selected->end(),
                        [](const World::TransportEdge *left, const World::TransportEdge *right) {
                          return left->Id < right->Id;
                        });
  World::CircuitRoute route{.SourceIdentity = graph.SourceIdentity(),
                            .SourceRouteId = relationId,
                            .StartNodeId = first->FromNodeId,
                            .EdgeIds = {}};
  route.EdgeIds.reserve(selected->size());
  std::unordered_set<const World::TransportEdge *> visited;
  visited.reserve(selected->size());
  const World::TransportEdge *current = first;
  for (size_t step = 0; step < selected->size(); ++step) {
    if (!visited.insert(current).second) {
      return std::unexpected(CircuitError{.Code = CircuitErrorCode::DisconnectedRoute,
                                          .SourceId = current->FromNodeId});
    }
    route.EdgeIds.push_back(current->Id);
    const auto next = outgoing.find(current->ToNodeId);
    if (next == outgoing.end() || !World::TransportTopology::CanContinue(*current, *next->second)) {
      return std::unexpected(
          CircuitError{.Code = CircuitErrorCode::DisconnectedRoute, .SourceId = current->ToNodeId});
    }
    current = next->second;
  }
  if (current != first || visited.size() != selected->size()) {
    return std::unexpected(
        CircuitError{.Code = CircuitErrorCode::DisconnectedRoute, .SourceId = relationId});
  }
  return route;
}

}
