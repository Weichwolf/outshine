#include "RoadRefinementCoverage.h"
#include "RoadTerrainContact.h"

#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <vector>

namespace outshine {
namespace {

constexpr size_t kMaximumCorridors = 512;
constexpr double kRoadGroundMarginM = 20.0;

[[nodiscard]] EastNorth InFrame(const World::TransportNode &node, const TangentFrame &frame) {
  const EastNorthUp placed = frame.ToLocalPosition(
      {.LongitudeDeg = node.LongitudeDeg, .LatitudeDeg = node.LatitudeDeg, .HeightM = 0.0});
  return {.EastM = placed.EastM, .NorthM = placed.NorthM};
}

}

std::expected<std::vector<Generators::TerrainRefinementCorridor>, std::string>
RoadRefinementCoverage::Build(const World::TransportNetworkSnapshot &source,
                              std::span<const size_t> routeIndices,
                              const TangentFrame &frame) {
  std::vector<Generators::TerrainRefinementCorridor> corridors;
  const World::TransportTopology &topology = source.Topology();
  for (const size_t routeIndex : routeIndices) {
    if (routeIndex >= source.Routes().size()) {
      return std::unexpected(std::format("road refinement route {} is missing", routeIndex));
    }
    const World::NamedCircuitRoute &route = source.Routes()[routeIndex];
    for (const World::TransportEdgeId edgeId : route.Circuit.EdgeIds) {
      if (corridors.size() >= kMaximumCorridors) {
        return std::unexpected("road refinement exceeds its 512-edge budget");
      }
      const World::TransportEdge *edge = topology.FindEdge(edgeId);
      if (edge == nullptr) {
        return std::unexpected(std::format("road refinement route '{}' lost way {} segment {}",
                                           route.Id,
                                           edgeId.WayId,
                                           edgeId.SegmentOrdinal));
      }
      const World::TransportNode *from = topology.FindNode(edge->FromNodeId);
      const World::TransportNode *to = topology.FindNode(edge->ToNodeId);
      if (from == nullptr || to == nullptr || !std::isfinite(edge->WidthM) || edge->WidthM <= 0.0) {
        return std::unexpected(
            std::format("road refinement route '{}' has invalid way {}", route.Id, edgeId.WayId));
      }
      corridors.push_back({.Start = InFrame(*from, frame),
                           .End = InFrame(*to, frame),
                           .HalfWidthM = edge->WidthM * 0.5 + kRoadGroundMarginM,
                           .MaximumPostingM = Generators::RoadTerrainContact::MaximumPostingM});
    }
  }
  return corridors;
}

}
