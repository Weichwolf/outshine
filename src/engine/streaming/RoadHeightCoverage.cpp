#include "RoadHeightCoverage.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <format>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "RoadTerrainPinJob.h"

namespace outshine {

std::expected<RoadHeightCoverage, std::string>
RoadHeightCoverage::Select(const World::TransportNetworkSnapshot &source, Budget budget) {
  RoadHeightCoverage result;
  const auto key = [](Data::TileId tile) { return std::tuple(tile.Zoom, tile.X, tile.Y); };
  for (size_t routeIndex = 0; routeIndex < source.Routes().size(); ++routeIndex) {
    const World::NamedCircuitRoute &named = source.Routes()[routeIndex];
    const World::CircuitRoute &route = named.Circuit;
    if (result.SelectedRouteIndices.size() >= budget.MaximumRoutes ||
        result.SelectedEdges > budget.MaximumEdges ||
        route.EdgeIds.size() > budget.MaximumEdges - result.SelectedEdges) {
      ++result.DeferredRoutes;
      continue;
    }
    auto selected =
        RoadTerrainPinJob::SelectTiles(source.Topology(),
                                       route.SourceIdentity,
                                       route.EdgeIds,
                                       {.Zoom = budget.Zoom, .MaximumTiles = budget.MaximumTiles});
    if (!selected) {
      if (selected.error().Code == RoadTerrainPinErrorCode::TooManyTiles) {
        ++result.DeferredRoutes;
        continue;
      }
      return std::unexpected(std::format("semantic road route '{}' has invalid tile coverage ({})",
                                         named.Id,
                                         static_cast<int>(selected.error().Code)));
    }
    std::vector<Data::TileId> combined = result.Tiles;
    combined.insert(combined.end(), selected->begin(), selected->end());
    std::ranges::sort(combined, {}, key);
    combined.erase(std::ranges::unique(combined).begin(), combined.end());
    if (combined.size() > budget.MaximumTiles) {
      ++result.DeferredRoutes;
      continue;
    }
    result.Tiles = std::move(combined);
    result.SelectedRouteIndices.push_back(routeIndex);
    result.SelectedEdges += route.EdgeIds.size();
  }
  return result;
}

}
