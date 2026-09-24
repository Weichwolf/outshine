#include "RoadAlignmentBuildQueue.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "RoadConstraintChain.h"
#include "RoadTerrainPinJob.h"

namespace outshine {
namespace {

[[nodiscard]] RoadAlignmentBuildError Error(RoadAlignmentBuildErrorCode code,
                                            std::string routeId = {},
                                            std::optional<Data::TileId> tile = std::nullopt,
                                            World::TransportEdgeId edge = {}) {
  return {.Code = code, .RouteId = std::move(routeId), .Tile = tile, .Edge = edge};
}

}

RoadAlignmentBuildQueue::Result RoadAlignmentBuildQueue::Build(RoadAlignmentBuildRequest request) {
  constexpr size_t kMaximumRoutes = 32;
  constexpr size_t kMaximumEdges = 512;
  if (!request.Source || request.RouteIndices.empty() ||
      request.RouteIndices.size() > kMaximumRoutes || request.CandidateGeneration == 0) {
    return std::unexpected(Error(RoadAlignmentBuildErrorCode::InvalidRequest));
  }
  RoadAlignmentBuildProduct product{.CandidateGeneration = request.CandidateGeneration,
                                    .SourceIdentity = request.Source->SourceIdentity(),
                                    .Routes = {}};
  product.Routes.reserve(request.RouteIndices.size());
  size_t selectedEdges = 0;
  for (const size_t routeIndex : request.RouteIndices) {
    if (routeIndex >= request.Source->Routes().size() ||
        std::count(request.RouteIndices.begin(), request.RouteIndices.end(), routeIndex) != 1) {
      return std::unexpected(Error(RoadAlignmentBuildErrorCode::InvalidRequest));
    }
    const World::NamedCircuitRoute &named = request.Source->Routes()[routeIndex];
    if (selectedEdges > kMaximumEdges ||
        named.Circuit.EdgeIds.size() > kMaximumEdges - selectedEdges) {
      return std::unexpected(Error(RoadAlignmentBuildErrorCode::InvalidRequest, named.Id));
    }
    selectedEdges += named.Circuit.EdgeIds.size();
    auto pin = RoadTerrainPinJob::Begin(request.Terrain,
                                        request.Source->Topology(),
                                        named.Circuit.SourceIdentity,
                                        named.Circuit.EdgeIds,
                                        {.Zoom = request.TerrainZoom,
                                         .MaximumTiles = 256,
                                         .CandidateGeneration = request.CandidateGeneration});
    if (!pin) {
      return std::unexpected(
          Error(RoadAlignmentBuildErrorCode::TerrainPin, named.Id, std::nullopt, pin.error().Edge));
    }
    if (!pin->Advance(pin->TileCount())) {
      return std::unexpected(
          Error(RoadAlignmentBuildErrorCode::MissingTerrain, named.Id, pin->PendingTile()));
    }
    const std::optional<PinnedRoadTerrain> &terrain = pin->Result();
    if (!terrain) {
      return std::unexpected(Error(RoadAlignmentBuildErrorCode::MissingTerrain, named.Id));
    }
    auto constraints = Generators::RoadConstraintChain::Build(request.Source->Topology(),
                                                              named.Circuit.SourceIdentity,
                                                              named.Circuit.EdgeIds,
                                                              *terrain->Heights);
    if (!constraints) {
      return std::unexpected(Error(RoadAlignmentBuildErrorCode::Constraints,
                                   named.Id,
                                   std::nullopt,
                                   constraints.error().Edge));
    }
    auto alignment = Generators::RoadAlignmentBuilder::Build(*constraints);
    if (!alignment) {
      return std::unexpected(Error(RoadAlignmentBuildErrorCode::Alignment,
                                   named.Id,
                                   std::nullopt,
                                   alignment.error().SourceEdge));
    }
    product.Routes.push_back(
        {.Id = named.Id,
         .Alignment = std::make_shared<const Generators::RoadAlignment>(std::move(*alignment))});
  }
  return product;
}

bool RoadAlignmentBuildQueue::TryStart(Tasks &tasks, RoadAlignmentBuildRequest request) {
  if (Pending_ || Completed_) { return false; }
  const uint64_t generation = request.CandidateGeneration;
  auto output = std::make_shared<std::optional<Result>>();
  const Tasks::Handle handle = tasks.Post([request = std::move(request), output] mutable {
    output->emplace(Build(std::move(request)));
  });
  Pending_.emplace(
      Pending{.Handle = handle, .CandidateGeneration = generation, .Output = std::move(output)});
  return true;
}

void RoadAlignmentBuildQueue::Poll(Tasks &tasks, uint64_t activeCandidateGeneration) {
  if (Completed_ && CompletedGeneration_ != activeCandidateGeneration) { Completed_.reset(); }
  if (!Pending_ || !tasks.Done(Pending_->Handle)) { return; }
  const Pending finished = std::move(*Pending_);
  Pending_.reset();
  if (finished.CandidateGeneration != activeCandidateGeneration) { return; }
  CompletedGeneration_ = finished.CandidateGeneration;
  if (!finished.Output->has_value()) {
    Completed_.emplace(std::unexpected(Error(RoadAlignmentBuildErrorCode::InvalidRequest)));
    return;
  }
  Completed_ = std::move(*finished.Output);
}

std::optional<RoadAlignmentBuildQueue::Result> RoadAlignmentBuildQueue::TakeCompleted() {
  return std::exchange(Completed_, std::nullopt);
}

}
