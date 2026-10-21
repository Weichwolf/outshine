#include "StructureBuildQueue.h"
#include "PreparedBuildingAssets.h"
#include "Shape.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <memory>
#include <utility>

namespace outshine {
bool StructureBuildQueue::CellRequestsValid(
    std::span<const CellRequest> requests,
    const Generators::Osm::BuildingField::AcceptedInput &input) const noexcept {
  if (requests.empty() || !input.Qualified || input.SourceKey == 0) { return false; }
  const auto first = requests.front();
  uint64_t cells = 0;
  for (const auto request : requests) {
    if (request.Tile != first.Tile || request.SourceKey != input.SourceKey || request.Cell == 0 ||
        request.Cell > Generators::kStructureCellsPerTile ||
        request.Detail > LevelOfDetail::Massed || CellQueued(request)) {
      return false;
    }
    const uint64_t bit = uint64_t{1} << (request.Cell - 1u);
    if ((cells & bit) != 0 || (input.OccupiedCells & bit) == 0) { return false; }
    cells |= bit;
  }
  return true;
}

std::expected<size_t, std::string>
StructureBuildQueue::PostAssetCells(Ground::SurfacePreparation &stack,
                                    const Generators::Osm::BuildingField &footprints,
                                    LongitudeLatitude eye,
                                    const HeightSource &heightAt,
                                    std::span<const CellRequest> requests) {
  const auto *vectors = stack.Vectors();
  const auto first = requests.front();
  const auto *input = footprints.InputOfTile(first.Tile);
  if (vectors == nullptr || first.Tile >= vectors->Tiles().size() || input == nullptr ||
      !input->Qualified || !input->Bake.PreparedAssetKey ||
      input->HeightRevision != heightAt.Revision.Value ||
      QualifiedSourceKey(footprints, first.Tile) != first.SourceKey) {
    return 0;
  }
  VectorSelection selected;
  if (!SelectNativeBasis(stack, footprints, first.Tile, selected)) {
    if (NativeFailure_) {
      return std::unexpected(std::string(Generators::Describe(*NativeFailure_)));
    }
    return 0;
  }
  if (!selected.Basis || selected.Key != *input->Bake.PreparedAssetKey ||
      (!selected.Basis->Sources.empty() &&
       (!selected.Basis->HeightQualified || selected.Basis->FallbackHeights))) {
    return 0;
  }
  const size_t most = std::min(static_cast<size_t>(Pool_->Threads()), kCandidateWindow);
  size_t posted = 0;
  for (const auto request : requests) {
    if (Queue_.size() + CellQueue_.size() >= most) { break; }
    auto raw = Borrowed(IdleRaw_);
    *raw = {};
    raw->Eye = eye;
    raw->EyeEcef = footprints.EyeEcef();
    raw->Projection = footprints.Projection();
    raw->RequestedDetail = request.Detail;
    raw->RequestedCell = request.Cell;
    raw->ClusterTriangles = Render::kClusterTriangles;
    auto output = Borrowed(IdleOut_);
    *output = {};
    output->CacheHit = true;
    output->CacheReadMs = selected.ReadMs;
    StructureBuildTask task(
        request.Tile, selected.Basis, std::move(raw), std::move(output), LentScratch());
    task.UseNativeAssets(stack.BuildingAssets(), selected.Key, selected.RequestKey);
    CellQueue_.push_back({.Revision = {.Vectors = vectors->Generation(),
                                       .HeightSource = heightAt.Revision,
                                       .Projection = footprints.Projection(),
                                       .TileSpanM = footprints.TileSpanM(),
                                       .Eye = eye,
                                       .EyeEcef = footprints.EyeEcef(),
                                       .RequestedDetail = request.Detail,
                                       .Purpose = BuildPurpose::ViewDetail},
                          .Task = std::move(task),
                          .StreetDigest = input->Bake.StreetDigest,
                          .SourceKey = request.SourceKey,
                          .Cell = request.Cell});
    PostSlice(CellQueue_.back());
    ++Posted_;
    ++posted;
  }
  return posted;
}
}
