#ifndef OUTSHINE_TEST_TRANSPORTCOREFIXTURE_H
#define OUTSHINE_TEST_TRANSPORTCOREFIXTURE_H

#include "TransportCoreSelection.h"
#include "TransportNetworkSnapshot.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

namespace outshine::Test {
inline constexpr uint64_t kCoreBridgePath = 100;
inline constexpr GeoCellId kTransportCore{.Level = 12, .X = 2048, .Y = 2048};
inline constexpr Generators::TransportCoreLimits kCoreLimits{.SourceNodesMost = 64,
                                                             .SourceEdgesMost = 64,
                                                             .SegmentsMost = 64,
                                                             .ContainerCapacityMost =
                                                                 size_t{64} * 1024};

inline std::shared_ptr<const World::TransportNetworkSnapshot>
CoreSource(std::vector<World::TransportNode> nodes,
           std::vector<World::TransportEdge> edges,
           std::vector<GeographicBounds> coverage = {}) {
  return std::make_shared<const World::TransportNetworkSnapshot>(
      World::ResolvedTransport{
          .Graph = World::TransportTopology(
              {.DatasetId = "native-core", .Revision = "1"}, std::move(nodes), std::move(edges)),
          .Routes = {}},
      std::move(coverage),
      World::TransportLoadMetrics{});
}

inline std::shared_ptr<const World::TransportNetworkSnapshot> BridgeAcrossCore(double extentDeg) {
  using namespace World;
  const auto bounds = kTransportCore.Bounds();
  if (!bounds) { return {}; }
  const double latitude = std::midpoint(bounds->SouthDeg, bounds->NorthDeg);
  const double longitude = std::midpoint(bounds->WestDeg, bounds->EastDeg);
  const double west = longitude - extentDeg;
  const double east = longitude + extentDeg;
  std::vector<TransportNode> nodes{
      {.SourceNodeId = 1, .LatitudeDeg = latitude, .LongitudeDeg = west},
      {.SourceNodeId = 2, .LatitudeDeg = latitude, .LongitudeDeg = east},
      {.SourceNodeId = 3, .LatitudeDeg = latitude, .LongitudeDeg = west - extentDeg},
      {.SourceNodeId = 4, .LatitudeDeg = latitude, .LongitudeDeg = east + extentDeg},
      {.SourceNodeId = 5, .LatitudeDeg = latitude + extentDeg, .LongitudeDeg = west},
      {.SourceNodeId = 6, .LatitudeDeg = latitude + extentDeg, .LongitudeDeg = east}};
  TransportEdge bridge{.Id = {.PathId = kCoreBridgePath},
                       .FromNodeId = 1,
                       .ToNodeId = 2,
                       .Modes = static_cast<uint8_t>(TransportMode::Motor),
                       .Facility = TransportFacility::LocalStreet,
                       .Surface = TransportSurface::Asphalt,
                       .WidthM = 8,
                       .LaneCount = 2,
                       .Layer = 1,
                       .Bridge = true,
                       .Access = TransportAccess::Restricted};
  std::vector<TransportEdge> edges{bridge};
  bridge.Id.Direction = EdgeDirection::Reverse;
  std::swap(bridge.FromNodeId, bridge.ToNodeId);
  bridge.Modes = static_cast<uint8_t>(TransportMode::Cycle);
  edges.push_back(bridge);
  constexpr uint64_t westGroundPath = 200;
  constexpr uint64_t eastGroundPath = 201;
  constexpr uint64_t unrelatedPath = 300;
  edges.push_back({.Id = {.PathId = westGroundPath}, .FromNodeId = 3, .ToNodeId = 1});
  edges.push_back({.Id = {.PathId = eastGroundPath}, .FromNodeId = 2, .ToNodeId = 4});
  edges.push_back({.Id = {.PathId = unrelatedPath}, .FromNodeId = 5, .ToNodeId = 6});
  return CoreSource(std::move(nodes), std::move(edges), {*bounds});
}
}
#endif
