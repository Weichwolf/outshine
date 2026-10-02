#include "OsmTransportBuilder.h"
#include "OsmWaySemantics.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <ranges>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {

namespace {

std::expected<void, TransportBuildError> AppendWayEdges(std::vector<World::TransportEdge> &edges,
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
    const auto append = [&](World::EdgeDirection direction, uint64_t start, uint64_t end) {
      edges.push_back(
          World::TransportEdge{.Id = {.PathId = way.Id,
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
    if (semantics.Travel != OsmWayTravel::Reverse) {
      append(World::EdgeDirection::Forward, from, to);
    }
    if (semantics.Travel != OsmWayTravel::Forward) {
      append(World::EdgeDirection::Reverse, to, from);
    }
  }
  return {};
}

}

std::expected<World::TransportTopology, TransportBuildError>
TransportBuilder::Build(const Data::OsmElements &source) {
  if (const auto missing = source.FirstMissingReference()) {
    return std::unexpected(TransportBuildError{.Code = TransportBuildErrorCode::MissingSourceObject,
                                               .SourceId = missing->OwnerId});
  }
  return BuildRegion(source);
}

std::expected<World::TransportTopology, TransportBuildError>
TransportBuilder::BuildRegion(const Data::OsmElements &source) {
  std::vector<World::TransportNode> nodes;
  std::vector<World::TransportEdge> edges;
  size_t unclassified = 0;
  nodes.reserve(source.Nodes().size());
  for (const Data::OsmNode &node : source.Nodes()) {
    nodes.push_back(World::TransportNode{.SourceNodeId = node.Id,
                                         .LatitudeDeg = node.LatitudeDeg,
                                         .LongitudeDeg = node.LongitudeDeg});
  }
  for (const Data::OsmWay &way : source.Ways()) {
    const auto described = DescribeOsmWay(way);
    if (!described) {
      return std::unexpected(TransportBuildError{.Code = described.error(), .SourceId = way.Id});
    }
    if (!described->TransportTagged) { continue; }
    if (std::ranges::any_of(way.NodeIds,
                            [&source](uint64_t id) { return source.FindNode(id) == nullptr; })) {
      return std::unexpected(TransportBuildError{
          .Code = TransportBuildErrorCode::MissingSourceObject, .SourceId = way.Id});
    }
    if (described->Modes == 0) { ++unclassified; }
    if (const auto appended = AppendWayEdges(edges, way, *described); !appended) {
      return std::unexpected(appended.error());
    }
  }
  return World::TransportTopology(
      source.SourceIdentity(), std::move(nodes), std::move(edges), unclassified);
}

}
