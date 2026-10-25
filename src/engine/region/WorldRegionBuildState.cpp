#include "WorldRegionBuildState.h"
#include "StructureCell.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ratio>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine {

WorldRegionBuildState::WorldRegionBuildState(
    Render::SceneRenderer &renderer,
    const WorldRegionState &world,
    const ::outshine::Generators::Osm::BuildingField &footprints,
    Around coverage,
    GroundRevision revision,
    uint64_t id)
    : Coverage_(coverage),
      Revision_(revision),
      Candidate_(renderer, world, footprints),
      TransportSnapshot_(world.CurrentTransportReady() ? world.OsmTransport->Current() : nullptr),
      Id_(id) {
  if (!world.CurrentOriginalReady()) { return; }
  if (world.OsmSource->Current()) {
    OriginalSources_.push_back(world.OsmSource->Current());
  } else {
    for (const auto &cell : world.OsmSource->CurrentCells()) {
      if (cell.Product) {
        OriginalCells_.push_back(
            std::static_pointer_cast<const Generators::Osm::StructureCell>(cell.Product));
      } else {
        OriginalSources_.push_back(cell.Snapshot);
      }
    }
  }
  const auto accepted = Footprints().AcceptedTiles();
  const auto sameLayout = [&] {
    if (accepted.empty()) { return true; }
    const size_t count = OriginalSources_.size() + OriginalCells_.size();
    if (accepted.size() != count) { return false; }
    for (size_t tile = 0; tile < count; ++tile) {
      const auto *input = Footprints().InputOfTile(static_cast<uint32_t>(tile));
      if (!input || !input->Coordinates || !input->Coordinates->Origin.Provenance ||
          input->Coordinates->Origin.Provenance->Cell !=
              (OriginalCells_.empty()
                   ? OriginalSources_[tile]->Cell
                   : OriginalCells_[tile]->Description.Footprints.Origin.Provenance->Cell)) {
        return false;
      }
    }
    return true;
  }();
  if (sameLayout) { return; }
  for (const auto tile : accepted) { Candidate_.Products().Pieces.Retire(tile); }
  Footprints().ResetDerived();
}

void WorldRegionBuildState::AddStructureHeightCoverage(std::span<const Data::TileId> tiles) {
  auto &all = RoadHeightCoverage_.Tiles;
  all.insert(all.end(), tiles.begin(), tiles.end());
  std::ranges::sort(all, [](const auto &a, const auto &b) {
    return std::tie(a.Zoom, a.X, a.Y) < std::tie(b.Zoom, b.X, b.Y);
  });
  all.erase(std::ranges::unique(all).begin(), all.end());
}

[[nodiscard]] std::expected<bool, std::string>
WorldRegionBuildState::PrepareOriginalHeights(StructureBuildQueue &queue, int zoom) {
  const auto prepared =
      !OriginalCells_.empty()
          ? queue.PrepareOriginal(OriginalCells_, zoom)
          : queue.PrepareOriginal(OriginalSources_, Generators::Osm::kDefaultStructurePolicy, zoom);
  if (!prepared) { return std::unexpected(prepared.error()); }
  if (!*prepared) { return false; }
  const auto tiles = queue.OriginalHeightTiles(zoom);
  if (!tiles) { return std::unexpected(tiles.error()); }
  AddStructureHeightCoverage(*tiles);
  return true;
}

void WorldRegionBuildState::PublishesWaterInputs(const Generators::WaterAsset &water) {
  Revision_.WaterTiles = water.IngestedTiles();
  Candidate_.RestoreWater(water);
  WaterPhase_ = WaterPhase::Bound;
  WaterWorker_.reset();
}

[[nodiscard]] std::optional<std::expected<WaterGeometryPreparation::Completed, std::string>>
WorldRegionBuildState::AdvanceWater(Tasks &pool,
                                    const Generators::WaterAsset &source,
                                    const TangentFrame &frame) {
  if (WaterWorker_ && !WaterWorker_->Matches(source)) {
    WaterWorker_->Cancel();
    if (!WaterWorker_->Complete()) { return std::nullopt; }
    WaterWorker_.reset();
  }
  if (!WaterWorker_) {
    WaterWorker_ = std::make_unique<WaterGeometryPreparation>(pool, source, frame);
    WaterPhase_ = WaterPhase::Preparing;
    return std::nullopt;
  }
  return WaterWorker_->Collect();
}

[[nodiscard]] bool WorldRegionBuildState::InstallRegion(GroundRegionAsset region, bool hit) {
  if (hit && !Schedule_.LoadReadyRegion()) { return false; }
  auto &build = Candidate_.Products();
  Lays(std::move(region.Terrain));
  build.Ground = std::move(region.Surfaces);
  SurfaceManifestKey_ = std::move(region.SurfaceManifest);
  RoadRestoration_.reset();
  RoadsRestored_ = false;
  build.TerrainClassificationMap = std::move(region.Classes);
  build.ClassPalette = std::move(region.ClassPalette);
  build.ClassUpload = build.TerrainClassificationMap
                          ? std::make_shared<const Render::PackedTerrainClassification>(
                                *build.TerrainClassificationMap)
                          : nullptr;
  build.StreetGraph = std::move(region.Network);
  build.StreetGraphWayCount = static_cast<size_t>(region.NetworkWays);
  build.RimsMissing = static_cast<size_t>(region.MissingRims);
  build.GroundSurface = MaterialInstance(region.GroundSurface);
  build.GroundMaterial = build.Ground.surfaceAt(build.GroundSurface);
  build.Sheets.Framed(TangentFrame::At(region.Anchor));
  Candidate_.Grounding(region.GroundAlbedo);
  return true;
}

[[nodiscard]] bool WorldRegionBuildState::AdvancesRetirement(size_t sheetsMost) noexcept {
  if (WaterWorker_) {
    WaterWorker_->Cancel();
    if (!WaterWorker_->Complete()) { return false; }
    WaterWorker_.reset();
  }
  if (StreetGraphWorker_) {
    StreetGraphWorker_->Cancel();
    if (!StreetGraphWorker_->Complete()) { return false; }
    StreetGraphWorker_.reset();
  }
  if (CorridorJob_) {
    if (!CorridorJob_->RetireStep(kCorridorRetireUnitsPerFrame)) { return false; }
    CorridorJob_.reset();
  }
  if (Patchwork_) {
    const size_t count = std::min(sheetsMost, Patchwork_->Sheets.size());
    for (size_t released = 0; released < count; ++released) { Patchwork_->Sheets.pop_back(); }
    if (!Patchwork_->Sheets.empty()) { return false; }
    Patchwork_.reset();
  }
  return true;
}

[[nodiscard]] bool
WorldRegionBuildState::AdvancesCorridorRetirement(std::chrono::steady_clock::time_point began) {
  const auto retireAt = std::chrono::steady_clock::now();
  const bool retired = CorridorJob_->RetireStep(kCorridorRetireUnitsPerFrame);
  if (retired) { FinishesCorridors(); }
  SamplesCorridorRetirement(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - retireAt)
          .count());
  SamplesCorridorSlice(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  return retired;
}

void WorldRegionBuildState::SamplesClassComponents(
    const Render::GroundClassUploadMetrics &seen) noexcept {
  LongestClassComponents_.Storage.BytesSubmitted =
      std::max(LongestClassComponents_.Storage.BytesSubmitted, seen.Storage.BytesSubmitted);
  LongestClassComponents_.Storage.AllocationMs =
      std::max(LongestClassComponents_.Storage.AllocationMs, seen.Storage.AllocationMs);
  LongestClassComponents_.Storage.StagingMs =
      std::max(LongestClassComponents_.Storage.StagingMs, seen.Storage.StagingMs);
  LongestClassComponents_.Storage.SubmissionMs =
      std::max(LongestClassComponents_.Storage.SubmissionMs, seen.Storage.SubmissionMs);
  LongestClassComponents_.SourcePreparationMs =
      std::max(LongestClassComponents_.SourcePreparationMs, seen.SourcePreparationMs);
  LongestClassComponents_.RestoreSourceMs =
      std::max(LongestClassComponents_.RestoreSourceMs, seen.RestoreSourceMs);
}

[[nodiscard]] uint8_t WorldRegionBuildState::Progress() const noexcept {
  if (!Schedule_.Prepared()) { return 0; }
  if (!Patchwork_) { return 1; }
  if (Schedule_.CurrentSheetPhase() != Core::GroundBuildSchedule::SheetPhase::Ready) {
    return static_cast<uint8_t>(Schedule_.CurrentSheetPhase()) + 2u;
  }
  return static_cast<uint8_t>(Schedule_.CurrentStage()) + 6u;
}

[[nodiscard]] WorldRegionBuildState::ProductMemory
WorldRegionBuildState::CurrentProductMemory() const noexcept {
  return {
      .Candidate = BuildingEarthworks_ ? StampingCandidateHeapBytes_ : Candidate_.OwnedHeapBytes(),
      .TerrainSources = TerrainInputs_.HeapBytes(),
      .Patchwork = Patchwork_ ? Patchwork_->HeapBytes() : 0u,
      .SourceCoverage = RoadHeightCoverage_.Tiles.capacity() * sizeof(Data::TileId) +
                        RoadHeightCoverage_.SelectedRouteIndices.capacity() * sizeof(size_t),
      .Survey = Surveying_.Terrain.ProbePositionsM.capacity() * sizeof(Vec3f),
      .BuildingStamps = BuildingEarthworks_ ? BuildingEarthworks_->HeapBytes() : 0u,
      .Deformation = TerrainDeformation_ ? TerrainDeformation_->HeapBytes() : 0u,
      .CorridorStamps = Corridors_.capacity() * sizeof(EarthworkStamp) + CorridorHeapBytes_,
      .RoadStamps = RoadEarthworkHeapBytes_ + RoadEarthworks_.capacity() * sizeof(EarthworkStamp)};
}
}
