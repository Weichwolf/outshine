#include "RoadTerrainPinJob.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine {
namespace {

constexpr size_t kMaximumRouteEdges = 65536;

[[nodiscard]] RoadTerrainPinError
Error(RoadTerrainPinErrorCode code, World::TransportEdgeId edge = {}, uint64_t nodeId = 0) {
  return {.Code = code, .Edge = edge, .SourceNodeId = nodeId};
}

[[nodiscard]] Data::TileId TileAt(const World::TransportNode &node, int zoom) {
  const Ground::TileSpot spot = Ground::HeightField::SpotOf(
      {.LongitudeDeg = node.LongitudeDeg, .LatitudeDeg = node.LatitudeDeg}, zoom);
  return {
      .Zoom = spot.Zoom, .X = static_cast<uint32_t>(spot.X), .Y = static_cast<uint32_t>(spot.Y)};
}

}

std::expected<std::vector<Data::TileId>, RoadTerrainPinError>
RoadTerrainPinJob::SelectTiles(const World::TransportTopology &topology,
                               const Data::OsmSourceIdentity &selectionSource,
                               std::span<const World::TransportEdgeId> route,
                               RoadTerrainTileSelectionRequest request) {
  if (topology.SourceIdentity() != selectionSource) {
    return std::unexpected(Error(RoadTerrainPinErrorCode::SourceMismatch));
  }
  if (request.Zoom < 0 || request.Zoom > Ground::HeightField::MaximumTileZoom) {
    return std::unexpected(Error(RoadTerrainPinErrorCode::InvalidZoom));
  }
  if (route.empty()) { return std::unexpected(Error(RoadTerrainPinErrorCode::EmptyRoute)); }
  if (route.size() > kMaximumRouteEdges) {
    return std::unexpected(Error(RoadTerrainPinErrorCode::TooManyEdges));
  }
  std::vector<Data::TileId> tiles;
  tiles.reserve(std::min(request.MaximumTiles, route.size() * 2));
  for (const World::TransportEdgeId id : route) {
    const World::TransportEdge *edge = topology.FindEdge(id);
    if (edge == nullptr) {
      return std::unexpected(Error(RoadTerrainPinErrorCode::MissingEdge, id));
    }
    for (const uint64_t nodeId : {edge->FromNodeId, edge->ToNodeId}) {
      const World::TransportNode *node = topology.FindNode(nodeId);
      if (node == nullptr) {
        return std::unexpected(Error(RoadTerrainPinErrorCode::MissingNode, id, nodeId));
      }
      tiles.push_back(TileAt(*node, request.Zoom));
    }
  }
  std::ranges::sort(tiles, [](Data::TileId left, Data::TileId right) {
    return std::tie(left.Zoom, left.X, left.Y) < std::tie(right.Zoom, right.X, right.Y);
  });
  tiles.erase(std::ranges::unique(tiles).begin(), tiles.end());
  if (tiles.size() > request.MaximumTiles) {
    return std::unexpected(Error(RoadTerrainPinErrorCode::TooManyTiles));
  }
  return tiles;
}

std::expected<RoadTerrainPinJob, RoadTerrainPinError>
RoadTerrainPinJob::Begin(SourcedTerrainFields fields,
                         const World::TransportTopology &topology,
                         const Data::OsmSourceIdentity &selectionSource,
                         std::span<const World::TransportEdgeId> route,
                         RoadTerrainPinRequest request) {
  auto tiles = SelectTiles(topology,
                           selectionSource,
                           route,
                           {.Zoom = request.Zoom, .MaximumTiles = request.MaximumTiles});
  if (!tiles) { return std::unexpected(tiles.error()); }
  RoadTerrainPinJob job;
  job.Fields_ = std::move(fields);
  job.SourceIdentity_ = selectionSource;
  job.CandidateGeneration_ = request.CandidateGeneration;
  job.Zoom_ = request.Zoom;
  job.Tiles_ = std::move(*tiles);
  job.Blocks_.reserve(job.Tiles_.size());
  return job;
}

bool RoadTerrainPinJob::Advance(size_t tilesMost) {
  if (Result_) { return true; }
  for (size_t copied = 0; copied < tilesMost && NextTile_ < Tiles_.size(); ++copied) {
    Ground::HeightField::Block block;
    if (!Fields_.CopySourcedField(Tiles_[NextTile_], block) || block.Sources.empty()) {
      return false;
    }
    Blocks_.push_back(std::move(block));
    ++NextTile_;
  }
  if (NextTile_ != Tiles_.size()) { return false; }
  auto heights = Ground::HeightField::Of(Zoom_, std::move(Blocks_));
  Result_ = PinnedRoadTerrain{.CandidateGeneration = CandidateGeneration_,
                              .SourceIdentity = std::move(SourceIdentity_),
                              .Heights = std::move(heights)};
  return true;
}

std::optional<Data::TileId> RoadTerrainPinJob::PendingTile() const noexcept {
  if (Complete() || NextTile_ == Tiles_.size()) { return std::nullopt; }
  return Tiles_[NextTile_];
}

}
