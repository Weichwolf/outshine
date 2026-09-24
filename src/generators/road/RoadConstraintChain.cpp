#include "RoadConstraintChain.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <ranges>
#include <utility>

#include "TangentFrame.h"

namespace outshine::Generators {

namespace {

constexpr size_t kMaximumSelectedEdges = 65536;
constexpr double kMinimumHorizontalEdgeM = 0.001;

[[nodiscard]] RoadConstraintError
Error(RoadConstraintErrorCode code, World::TransportEdgeId edge = {}, uint64_t nodeId = 0) {
  return {.Code = code, .Edge = edge, .SourceNodeId = nodeId};
}

}

std::expected<RoadConstraintChain, RoadConstraintError>
RoadConstraintChain::Build(const World::TransportTopology &topology,
                           const Data::OsmSourceIdentity &selectionSource,
                           std::span<const World::TransportEdgeId> selectedEdges,
                           const Ground::HeightField &terrain) {
  if (topology.SourceIdentity() != selectionSource) {
    return std::unexpected(Error(RoadConstraintErrorCode::SourceMismatch));
  }
  if (!terrain.Qualified()) {
    return std::unexpected(Error(RoadConstraintErrorCode::UnqualifiedTerrain));
  }
  if (selectedEdges.empty()) {
    return std::unexpected(Error(RoadConstraintErrorCode::EmptySelection));
  }
  if (selectedEdges.size() > kMaximumSelectedEdges) {
    return std::unexpected(Error(RoadConstraintErrorCode::TooManyEdges));
  }
  std::vector<World::TransportEdgeId> unique(selectedEdges.begin(), selectedEdges.end());
  std::ranges::sort(unique);
  const auto repeated = std::ranges::adjacent_find(unique);
  if (repeated != unique.end()) {
    return std::unexpected(Error(RoadConstraintErrorCode::DuplicateEdge, *repeated));
  }

  RoadConstraintChain built;
  built.SourceIdentity_ = selectionSource;
  built.TerrainDigest_ = terrain.RasterDigest();
  built.TerrainSources_.assign(terrain.Sources().begin(), terrain.Sources().end());
  built.Points_.reserve(selectedEdges.size() + 1);
  built.Edges_.reserve(selectedEdges.size());

  const World::TransportEdge *first = topology.FindEdge(selectedEdges.front());
  if (first == nullptr) {
    return std::unexpected(Error(RoadConstraintErrorCode::MissingEdge, selectedEdges.front()));
  }
  const World::TransportNode *anchor = topology.FindNode(first->FromNodeId);
  if (anchor == nullptr) {
    return std::unexpected(
        Error(RoadConstraintErrorCode::MissingNode, first->Id, first->FromNodeId));
  }
  built.Anchor_ = {.LongitudeDeg = anchor->LongitudeDeg, .LatitudeDeg = anchor->LatitudeDeg};
  const TangentFrame frame = TangentFrame::At(built.Anchor_);

  const auto appendPoint =
      [&](uint64_t nodeId,
          World::TransportEdgeId edgeId) -> std::expected<void, RoadConstraintError> {
    const World::TransportNode *node = topology.FindNode(nodeId);
    if (node == nullptr) {
      return std::unexpected(Error(RoadConstraintErrorCode::MissingNode, edgeId, nodeId));
    }
    const LongitudeLatitude geographic{.LongitudeDeg = node->LongitudeDeg,
                                       .LatitudeDeg = node->LatitudeDeg};
    const std::optional<double> elevation = terrain.At(geographic).AslM();
    if (!elevation) {
      return std::unexpected(Error(RoadConstraintErrorCode::MissingTerrain, edgeId, nodeId));
    }
    const EastNorthUp local = frame.ToLocalPosition({.LongitudeDeg = geographic.LongitudeDeg,
                                                     .LatitudeDeg = geographic.LatitudeDeg,
                                                     .HeightM = *elevation});
    if (!std::isfinite(local.EastM) || !std::isfinite(local.NorthM) || !std::isfinite(local.UpM)) {
      return std::unexpected(Error(RoadConstraintErrorCode::DegenerateGeometry, edgeId, nodeId));
    }
    built.Points_.push_back({.SourceNodeId = nodeId,
                             .Geographic = geographic,
                             .TerrainElevationM = *elevation,
                             .TerrainLocalM = local});
    return {};
  };

  for (const World::TransportEdgeId edgeId : selectedEdges) {
    const World::TransportEdge *edge = topology.FindEdge(edgeId);
    if (edge == nullptr) {
      return std::unexpected(Error(RoadConstraintErrorCode::MissingEdge, edgeId));
    }
    if (!edge->Allows(World::TransportMode::Motor) || !(edge->WidthM > 0.0)) {
      return std::unexpected(Error(RoadConstraintErrorCode::UnusableEdge, edgeId));
    }
    if (built.Points_.empty()) {
      if (const auto appended = appendPoint(edge->FromNodeId, edgeId); !appended) {
        return std::unexpected(appended.error());
      }
    } else if (built.Points_.back().SourceNodeId != edge->FromNodeId) {
      return std::unexpected(
          Error(RoadConstraintErrorCode::DisconnectedEdge, edgeId, edge->FromNodeId));
    }
    if (const auto appended = appendPoint(edge->ToNodeId, edgeId); !appended) {
      return std::unexpected(appended.error());
    }
    const RoadConstraintPoint &from = built.Points_[built.Points_.size() - 2];
    const RoadConstraintPoint &to = built.Points_.back();
    const double lengthM = std::hypot(to.TerrainLocalM.EastM - from.TerrainLocalM.EastM,
                                      to.TerrainLocalM.NorthM - from.TerrainLocalM.NorthM);
    if (!std::isfinite(lengthM) || lengthM < kMinimumHorizontalEdgeM ||
        !std::isfinite(built.EstimatedLengthM_ + lengthM)) {
      return std::unexpected(
          Error(RoadConstraintErrorCode::DegenerateGeometry, edgeId, edge->ToNodeId));
    }
    built.EstimatedLengthM_ += lengthM;
    built.Edges_.push_back({.SourceEdge = edgeId,
                            .FromPointIndex = static_cast<uint32_t>(built.Edges_.size()),
                            .ToPointIndex = static_cast<uint32_t>(built.Edges_.size() + 1),
                            .Facility = edge->Facility,
                            .Surface = edge->Surface,
                            .WidthM = edge->WidthM,
                            .Layer = edge->Layer,
                            .Bridge = edge->Bridge,
                            .Tunnel = edge->Tunnel});
  }
  built.Closed_ = built.Points_.front().SourceNodeId == built.Points_.back().SourceNodeId;
  return built;
}

}
