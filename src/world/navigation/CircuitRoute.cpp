#include "TransportTopology.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace outshine::World {

namespace {

bool IsCircuit(const Data::OsmRelation &relation) {
  size_t kinds = 0;
  for (const Data::OsmTag &tag : relation.Tags) {
    if (tag.Key != "type") { continue; }
    ++kinds;
    if (tag.Value != "circuit") { return false; }
  }
  return kinds == 1;
}

std::expected<const TransportEdge *, CircuitError>
ResolveSegment(const TransportTopology &graph, const Data::OsmWay &way, size_t segment) {
  const TransportEdgeId forwardId{.WayId = way.Id,
                                  .SegmentOrdinal = static_cast<uint32_t>(segment),
                                  .Direction = EdgeDirection::Forward};
  const TransportEdgeId reverseId{.WayId = way.Id,
                                  .SegmentOrdinal = static_cast<uint32_t>(segment),
                                  .Direction = EdgeDirection::Reverse};
  const TransportEdge *forward = graph.FindEdge(forwardId);
  const TransportEdge *reverse = graph.FindEdge(reverseId);
  if (forward != nullptr && reverse != nullptr) {
    return std::unexpected(
        CircuitError{.Code = CircuitErrorCode::AmbiguousDirection, .SourceId = way.Id});
  }
  const TransportEdge *edge = forward != nullptr ? forward : reverse;
  if (edge == nullptr) {
    return std::unexpected(CircuitError{.Code = CircuitErrorCode::MissingEdge, .SourceId = way.Id});
  }
  const uint64_t expectedFrom =
      forward != nullptr ? way.NodeIds[segment] : way.NodeIds[segment + 1];
  const uint64_t expectedTo = forward != nullptr ? way.NodeIds[segment + 1] : way.NodeIds[segment];
  if (edge->FromNodeId != expectedFrom || edge->ToNodeId != expectedTo) {
    return std::unexpected(CircuitError{.Code = CircuitErrorCode::MissingEdge, .SourceId = way.Id});
  }
  if (edge->Modes == 0 || edge->Access == TransportAccess::Forbidden) {
    return std::unexpected(
        CircuitError{.Code = CircuitErrorCode::UnusableEdge, .SourceId = way.Id});
  }
  return edge;
}

std::expected<std::vector<const TransportEdge *>, CircuitError>
SelectEdges(const TransportTopology &graph,
            const Data::OsmElements &source,
            const Data::OsmRelation &relation,
            std::string_view memberRole) {
  std::vector<const TransportEdge *> selected;
  for (const Data::OsmRelationMember &member : relation.Members) {
    if (member.Role != memberRole) { continue; }
    if (member.Kind != Data::OsmElementKind::Way) {
      return std::unexpected(
          CircuitError{.Code = CircuitErrorCode::InvalidMember, .SourceId = member.Id});
    }
    const Data::OsmWay *way = source.FindWay(member.Id);
    if (way == nullptr) {
      return std::unexpected(
          CircuitError{.Code = CircuitErrorCode::MissingWay, .SourceId = member.Id});
    }
    if (way->NodeIds.size() < 2) {
      return std::unexpected(
          CircuitError{.Code = CircuitErrorCode::InvalidMember, .SourceId = member.Id});
    }
    for (size_t segment = 0; segment + 1 < way->NodeIds.size(); ++segment) {
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

std::expected<CircuitRoute, CircuitError> TransportTopology::ResolveCircuit(
    const Data::OsmElements &source, uint64_t relationId, std::string_view memberRole) const {
  if (source.SourceIdentity() != SourceIdentity_) {
    return std::unexpected(
        CircuitError{.Code = CircuitErrorCode::SourceMismatch, .SourceId = relationId});
  }
  const Data::OsmRelation *relation = source.FindRelation(relationId);
  if (relation == nullptr) {
    return std::unexpected(
        CircuitError{.Code = CircuitErrorCode::MissingRelation, .SourceId = relationId});
  }
  if (!IsCircuit(*relation)) {
    return std::unexpected(
        CircuitError{.Code = CircuitErrorCode::NotCircuit, .SourceId = relationId});
  }
  const auto selected = SelectEdges(*this, source, *relation, memberRole);
  if (!selected) { return std::unexpected(selected.error()); }
  std::unordered_map<uint64_t, const TransportEdge *> outgoing;
  outgoing.reserve(selected->size());
  for (const TransportEdge *edge : *selected) {
    if (!outgoing.emplace(edge->FromNodeId, edge).second) {
      return std::unexpected(
          CircuitError{.Code = CircuitErrorCode::AmbiguousDirection, .SourceId = edge->FromNodeId});
    }
  }
  const TransportEdge *first = *std::min_element(
      selected->begin(),
      selected->end(),
      [](const TransportEdge *left, const TransportEdge *right) { return left->Id < right->Id; });
  CircuitRoute route{.SourceIdentity = SourceIdentity_,
                     .RelationId = relationId,
                     .StartNodeId = first->FromNodeId,
                     .EdgeIds = {}};
  route.EdgeIds.reserve(selected->size());
  std::unordered_set<const TransportEdge *> visited;
  visited.reserve(selected->size());
  const TransportEdge *current = first;
  for (size_t step = 0; step < selected->size(); ++step) {
    if (!visited.insert(current).second) {
      return std::unexpected(CircuitError{.Code = CircuitErrorCode::DisconnectedRoute,
                                          .SourceId = current->FromNodeId});
    }
    route.EdgeIds.push_back(current->Id);
    const auto next = outgoing.find(current->ToNodeId);
    if (next == outgoing.end() || !CanContinue(*current, *next->second)) {
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
