#include "TransportTopology.h"
#include "OsmWaySemantics.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace outshine::World {

namespace {

std::expected<void, TransportBuildError> AppendWayEdges(std::vector<TransportEdge> &edges,
                                                        const Data::OsmWay &way,
                                                        const OsmWaySemantics &semantics) {
  for (size_t segment = 1; segment < way.NodeIds.size(); ++segment) {
    if (segment - 1 > std::numeric_limits<uint32_t>::max() ||
        edges.size() > std::numeric_limits<uint32_t>::max() - 2u) {
      return std::unexpected(
          TransportBuildError{.Code = TransportBuildErrorCode::TooManyEdges, .SourceId = way.Id});
    }
    const uint64_t from = way.NodeIds[segment - 1];
    const uint64_t to = way.NodeIds[segment];
    if (from == to) {
      return std::unexpected(TransportBuildError{.Code = TransportBuildErrorCode::DegenerateSegment,
                                                 .SourceId = way.Id});
    }
    const auto append = [&](EdgeDirection direction, uint64_t start, uint64_t end) {
      edges.push_back(TransportEdge{.Id = {.WayId = way.Id,
                                           .SegmentOrdinal = static_cast<uint32_t>(segment - 1),
                                           .Direction = direction},
                                    .FromNodeId = start,
                                    .ToNodeId = end,
                                    .Modes = semantics.Modes,
                                    .Facility = semantics.Facility,
                                    .Surface = semantics.Surface,
                                    .WidthM = semantics.WidthM,
                                    .LaneCount = semantics.LaneCount,
                                    .Layer = semantics.Layer,
                                    .Bridge = semantics.Bridge,
                                    .Tunnel = semantics.Tunnel,
                                    .Access = semantics.Access});
    };
    if (semantics.Travel != OsmWayTravel::Reverse) { append(EdgeDirection::Forward, from, to); }
    if (semantics.Travel != OsmWayTravel::Forward) { append(EdgeDirection::Reverse, to, from); }
  }
  return {};
}

}

std::expected<TransportTopology, TransportBuildError>
TransportTopology::Build(const Data::OsmElements &source) {
  if (const auto missing = source.FirstMissingReference()) {
    return std::unexpected(TransportBuildError{.Code = TransportBuildErrorCode::MissingSourceObject,
                                               .SourceId = missing->OwnerId});
  }
  TransportTopology built;
  built.SourceIdentity_ = source.SourceIdentity();
  built.Nodes_.reserve(source.Nodes().size());
  for (const Data::OsmNode &node : source.Nodes()) {
    built.Nodes_.push_back(TransportNode{.SourceNodeId = node.Id,
                                         .LatitudeDeg = node.LatitudeDeg,
                                         .LongitudeDeg = node.LongitudeDeg});
  }
  for (const Data::OsmWay &way : source.Ways()) {
    const auto described = DescribeOsmWay(way);
    if (!described) {
      return std::unexpected(TransportBuildError{.Code = described.error(), .SourceId = way.Id});
    }
    if (!described->TransportTagged) { continue; }
    if (described->Modes == 0) { ++built.UnclassifiedWayCount_; }
    if (const auto appended = AppendWayEdges(built.Edges_, way, *described); !appended) {
      return std::unexpected(appended.error());
    }
  }
  std::ranges::sort(built.Edges_, {}, &TransportEdge::Id);
  built.Outgoing_.reserve(built.Edges_.size());
  for (size_t index = 0; index < built.Edges_.size(); ++index) {
    built.Outgoing_.push_back(OutgoingTransportEdge{.NodeId = built.Edges_[index].FromNodeId,
                                                    .EdgeIndex = static_cast<uint32_t>(index)});
  }
  std::ranges::sort(built.Outgoing_,
                    [&](const OutgoingTransportEdge &left, const OutgoingTransportEdge &right) {
                      if (left.NodeId != right.NodeId) { return left.NodeId < right.NodeId; }
                      return built.Edges_[left.EdgeIndex].Id < built.Edges_[right.EdgeIndex].Id;
                    });
  return built;
}

const TransportNode *TransportTopology::FindNode(uint64_t id) const noexcept {
  const auto found = std::ranges::lower_bound(Nodes_, id, {}, &TransportNode::SourceNodeId);
  return found != Nodes_.end() && found->SourceNodeId == id ? &*found : nullptr;
}

const TransportEdge *TransportTopology::FindEdge(TransportEdgeId id) const noexcept {
  const auto found = std::ranges::lower_bound(Edges_, id, {}, &TransportEdge::Id);
  return found != Edges_.end() && found->Id == id ? &*found : nullptr;
}

std::span<const OutgoingTransportEdge>
TransportTopology::OutgoingFrom(uint64_t nodeId) const noexcept {
  const auto first =
      std::ranges::lower_bound(Outgoing_, nodeId, {}, &OutgoingTransportEdge::NodeId);
  const auto last = std::ranges::upper_bound(Outgoing_, nodeId, {}, &OutgoingTransportEdge::NodeId);
  return {first, last};
}

bool TransportTopology::CanContinue(const TransportEdge &from, const TransportEdge &to) noexcept {
  return from.ToNodeId == to.FromNodeId && (from.Modes & to.Modes) != 0 &&
         from.Access != TransportAccess::Forbidden && to.Access != TransportAccess::Forbidden;
}

}
