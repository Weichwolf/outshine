#ifndef OUTSHINE_TEST_TRANSPORTNETWORKFIXTURE_H
#define OUTSHINE_TEST_TRANSPORTNETWORKFIXTURE_H

#include "TransportNetworkSnapshot.h"
#include "TangentFrame.h"
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace outshine::Test {
inline std::shared_ptr<const World::TransportNetworkSnapshot>
TransportNetworkFixture(LongitudeLatitude anchor, GeographicBounds coverage) {
  using namespace World;
  const auto west = TangentFrame::At(anchor).ApproximateGeographicAt({.EastM = -100});
  std::vector<TransportNode> nodes{
      {.SourceNodeId = 7, .LatitudeDeg = anchor.LatitudeDeg, .LongitudeDeg = anchor.LongitudeDeg},
      {.SourceNodeId = 8, .LatitudeDeg = anchor.LatitudeDeg, .LongitudeDeg = coverage.EastDeg},
      {.SourceNodeId = 9, .LatitudeDeg = west.LatitudeDeg, .LongitudeDeg = west.LongitudeDeg}};
  constexpr uint64_t pathId = 42;
  TransportEdge edge{.Id = {.PathId = pathId},
                     .FromNodeId = 7,
                     .ToNodeId = 8,
                     .Modes = static_cast<uint8_t>(TransportMode::Motor) |
                              static_cast<uint8_t>(TransportMode::Walk) |
                              static_cast<uint8_t>(TransportMode::Cycle),
                     .Facility = TransportFacility::LocalStreet,
                     .Surface = TransportSurface::Asphalt,
                     .WidthM = 8,
                     .LaneCount = 2};
  std::vector<TransportEdge> edges{edge};
  edge.Id.Direction = EdgeDirection::Reverse;
  std::swap(edge.FromNodeId, edge.ToNodeId);
  edges.push_back(edge);
  edge.Id = {.PathId = pathId, .SegmentOrdinal = 1};
  edge.FromNodeId = 9;
  edge.ToNodeId = 7;
  edges.push_back(edge);
  edge.Id.Direction = EdgeDirection::Reverse;
  std::swap(edge.FromNodeId, edge.ToNodeId);
  edges.push_back(edge);
  const Data::SourceIdentity identity{.DatasetId = "fixture-source", .Revision = "snapshot-1"};
  constexpr size_t unclassified = 3;
  TransportTopology graph(identity, std::move(nodes), std::move(edges), unclassified);
  constexpr uint64_t routeId = 84;
  std::vector<NamedCircuitRoute> routes{
      {.Id = "fixture-circuit",
       .Circuit = {.SourceIdentity = identity,
                   .SourceRouteId = routeId,
                   .StartNodeId = 7,
                   .EdgeIds = {{.PathId = pathId},
                               {.PathId = pathId, .Direction = EdgeDirection::Reverse}}}}};
  constexpr size_t sourceBytes = 1234;
  return std::make_shared<const TransportNetworkSnapshot>(
      ResolvedTransport{.Graph = std::move(graph), .Routes = std::move(routes)},
      std::vector<GeographicBounds>{coverage},
      TransportLoadMetrics{.SourceBytes = sourceBytes, .ReadMs = 1, .ParseMs = 2, .GraphMs = 3});
}
}
#endif
