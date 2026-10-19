#ifndef OUTSHINE_ENGINE_REGION_WORLDREGIONBUILDSTATE_H
#define OUTSHINE_ENGINE_REGION_WORLDREGIONBUILDSTATE_H

#include "EngineState.h"
#include "Corridors.h"
#include "RoadHeightCoverage.h"
#include "StreetGraphPreparation.h"
#include "GroundWorldCandidate.h"
#include "GroundBuildSchedule.h"
#include "BuildingStampJob.h"
#include "TerrainDeformationTask.h"
#include "TerrainRefinement.h"
#include "TerrainMesh.h"
#include "WaterGeometryPreparation.h"
#include "WaterSurfaceBuilder.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
inline constexpr uint64_t kRegionDigestLowWordMask = 0xFFFFFFFFULL;
inline constexpr size_t kCorridorRetireUnitsPerFrame = 512;

class WorldRegionBuildState {
public:
  enum class GeometrySubmission : uint8_t { Classes, ClassRanges, Begin, Cook };
  enum class CorridorCompletion : uint8_t { Build, Retire };
  enum class RegionPhase : uint8_t { Lookup, Loading, Miss, Hit, Storing, Stored, Bypass };
  enum class WaterPhase : uint8_t { Awaiting, Preparing, Bound };

  struct ResidencyBuild {
    bool SheetsStitched = false;
    bool ResidencyStarted = false;
    bool ResidencyReady = false;
    double LongestResidencySliceMs = 0.0;
  };

  struct GroundSurvey {
    Generators::TerrainSurvey Terrain;
    size_t NextSheet = 0;
    double LongestSliceMs = 0.0;
  };

  WorldRegionBuildState(Render::SceneRenderer &renderer,
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
    for (const auto tile : accepted) { Candidate_.Products().Pieces.Forgets(tile); }
    Footprints().ResetDerived();
  }

  [[nodiscard]] bool Matches(const GroundRevision &revision) const noexcept {
    return RevisionDifference(revision) == 0;
  }

  [[nodiscard]] uint32_t RevisionDifference(const GroundRevision &revision) const noexcept {
    auto inputs = revision;
    if (WaterPhase_ != WaterPhase::Bound) { inputs.WaterTiles = Revision_.WaterTiles; }
    return Revision_.CandidateDifferenceMask(inputs);
  }

  [[nodiscard]] const Around &Coverage() const noexcept { return Coverage_; }

  [[nodiscard]] const GroundRevision &Revision() const noexcept { return Revision_; }

  [[nodiscard]] uint64_t Id() const noexcept { return Id_; }

  [[nodiscard]] GroundWorldCandidate &Candidate() noexcept { return Candidate_; }

  [[nodiscard]] const World::TransportNetworkSnapshot *TransportSnapshot() const noexcept {
    return TransportSnapshot_.get();
  }

  [[nodiscard]] const std::shared_ptr<const World::TransportNetworkSnapshot> &
  TransportSnapshotOwner() const noexcept {
    return TransportSnapshot_;
  }

  void SetRoadHeightCoverage(RoadHeightCoverage coverage) {
    RoadHeightCoverage_ = std::move(coverage);
  }

  [[nodiscard]] std::span<const Data::TileId> RoadHeightTiles() const noexcept {
    return RoadHeightCoverage_.Tiles;
  }

  void AddStructureHeightCoverage(std::span<const Data::TileId> tiles) {
    auto &all = RoadHeightCoverage_.Tiles;
    all.insert(all.end(), tiles.begin(), tiles.end());
    std::ranges::sort(all, [](const auto &a, const auto &b) {
      return std::tie(a.Zoom, a.X, a.Y) < std::tie(b.Zoom, b.X, b.Y);
    });
    all.erase(std::ranges::unique(all).begin(), all.end());
  }

  [[nodiscard]] std::expected<bool, std::string> PrepareOriginalHeights(StructureBuildQueue &queue,
                                                                        int zoom) {
    const auto prepared = !OriginalCells_.empty()
                              ? queue.PrepareOriginal(OriginalCells_, zoom)
                              : queue.PrepareOriginal(OriginalSources_,
                                                      Generators::Osm::kDefaultStructurePolicy,
                                                      zoom);
    if (!prepared) { return std::unexpected(prepared.error()); }
    if (!*prepared) { return false; }
    const auto tiles = queue.OriginalHeightTiles(zoom);
    if (!tiles) { return std::unexpected(tiles.error()); }
    AddStructureHeightCoverage(*tiles);
    return true;
  }

  [[nodiscard]] std::span<const size_t> RoadRouteIndices() const noexcept {
    return RoadHeightCoverage_.SelectedRouteIndices;
  }

  [[nodiscard]] bool RoadAlignmentRequested() const noexcept { return RoadAlignmentRequested_; }

  void MarkRoadAlignmentRequested() noexcept { RoadAlignmentRequested_ = true; }

  [[nodiscard]] bool RoadSurfaceBuilt() const noexcept { return RoadSurfaceBuilt_; }

  void MarkRoadSurfaceBuilt() noexcept { RoadSurfaceBuilt_ = true; }

  [[nodiscard]] size_t NextRoadSurfaceTransfer() const noexcept { return NextRoadSurfaceTransfer_; }

  void AdvanceRoadSurfaceTransfer() noexcept { ++NextRoadSurfaceTransfer_; }

  void HoldRoadEarthworks(std::vector<EarthworkStamp> stamps) {
    RoadEarthworkHeapBytes_ = StampHeapBytes(stamps);
    RoadEarthworks_ = std::move(stamps);
  }

  [[nodiscard]] std::vector<EarthworkStamp> TakeRoadEarthworks() noexcept {
    RoadEarthworkHeapBytes_ = 0;
    return std::exchange(RoadEarthworks_, {});
  }

  [[nodiscard]] ResidencyBuild &Residency() noexcept { return Residency_; }

  [[nodiscard]] GroundSurvey &Surveying() noexcept { return Surveying_; }

  [[nodiscard]] Generators::TerrainDeformationTask *Pressing() noexcept { return Pressing_.get(); }

  [[nodiscard]] Generators::BuildingStampJob *Stamping() noexcept { return Stamping_.get(); }

  void BeginsStamping(std::unique_ptr<Generators::BuildingStampJob> stamping) noexcept {
    StampingCandidateHeapBytes_ = Candidate_.OwnedHeapBytes();
    Stamping_ = std::move(stamping);
  }

  void FinishesStamping() noexcept { Stamping_.reset(); }

  void BeginsPressing(std::unique_ptr<Generators::TerrainDeformationTask> pressing) noexcept {
    Pressing_ = std::move(pressing);
  }

  void FinishesPressing() noexcept { Pressing_.reset(); }

  void SamplesPressingSlice(double milliseconds) noexcept {
    LongestPressingSliceMs_ = std::max(LongestPressingSliceMs_, milliseconds);
  }

  [[nodiscard]] double LongestPressingSliceMs() const noexcept { return LongestPressingSliceMs_; }

  [[nodiscard]] ::outshine::Generators::Osm::BuildingField &Footprints() noexcept {
    return Candidate_.Products().Footprints;
  }

  void PublishesFootprints() noexcept { Revision_.Footprints = Footprints().Revision(); }

  void PublishesWaterInputs(const Generators::WaterAsset &water) {
    Revision_.WaterTiles = water.IngestedTiles();
    Candidate_.RestoreWater(water);
    WaterPhase_ = WaterPhase::Bound;
    WaterWorker_.reset();
  }

  [[nodiscard]] bool HasWaterInputs() const noexcept { return WaterPhase_ == WaterPhase::Bound; }

  [[nodiscard]] std::optional<std::expected<WaterGeometryPreparation::Completed, std::string>>
  AdvanceWater(Tasks &pool, const Generators::WaterAsset &source, const TangentFrame &frame) {
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

  void DiscardWaterPreparation() {
    WaterWorker_.reset();
    WaterPhase_ = WaterPhase::Awaiting;
  }

  [[nodiscard]] Patchwork *Laid() noexcept { return Patchwork_ ? &*Patchwork_ : nullptr; }

  void Lays(Patchwork patchwork) noexcept { Patchwork_.emplace(std::move(patchwork)); }

  [[nodiscard]] RegionPhase RegionStatus() const noexcept { return RegionPhase_; }

  [[nodiscard]] GroundRegionPreparation *RegionWorker() const noexcept {
    return RegionWorker_.get();
  }

  [[nodiscard]] const std::string &RegionKey() const noexcept { return RegionKey_; }

  [[nodiscard]] const std::string &RegionRequest() const noexcept { return RegionRequest_; }

  [[nodiscard]] bool HasReadyRegion() const noexcept { return Schedule_.HasReadyRegion(); }

  void BeginRegion(Tasks &pool, GroundRegionPreparation::Factory factory, RegionPhase phase) {
    RegionWorker_ = std::make_unique<GroundRegionPreparation>(pool, std::move(factory));
    RegionPhase_ = phase;
  }

  void BeginRegion(std::unique_ptr<GroundRegionPreparation> worker, RegionPhase phase) {
    RegionWorker_ = std::move(worker);
    RegionPhase_ = phase;
  }

  void FinishRegion(std::string key, RegionPhase phase, std::string request = {}) {
    RegionWorker_.reset();
    RegionKey_ = std::move(key);
    RegionRequest_ = std::move(request);
    RegionPhase_ = phase;
  }

  [[nodiscard]] bool InstallRegion(GroundRegionAsset region, bool hit) {
    if (hit && !Schedule_.LoadReadyRegion()) { return false; }
    auto &build = Candidate_.Products();
    Lays(std::move(region.Terrain));
    build.Ground = std::move(region.Surfaces);
    build.ClassStructure = std::move(region.Classes);
    build.ClassPalette = std::move(region.ClassPalette);
    build.ClassUpload =
        build.ClassStructure
            ? std::make_shared<const Render::GroundClassBuffer>(*build.ClassStructure)
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

  [[nodiscard]] Core::GroundBuildSchedule::SheetPhase CurrentSheetPhase() const noexcept {
    return Schedule_.CurrentSheetPhase();
  }

  void AdvanceSheetPhase() noexcept {
    RecordsProductPeak();
    [[maybe_unused]] const bool completed = Schedule_.AdvanceSheetPhase();
    assert(completed);
  }

  [[nodiscard]] Core::GroundBuildSchedule::Stage CurrentStage() const noexcept {
    return Schedule_.CurrentStage();
  }

  void AdvanceStage() noexcept {
    RecordsProductPeak();
    [[maybe_unused]] const bool completed = Schedule_.AdvanceStage();
    assert(completed);
  }

  [[nodiscard]] size_t ProductPeakBytes() const noexcept { return ProductPeakBytes_; }

  [[nodiscard]] size_t RetainedProductBytes() const noexcept { return CurrentProductBytes(); }

  [[nodiscard]] size_t RetainedCandidateBytes() const noexcept {
    return Candidate_.OwnedHeapBytes();
  }

  [[nodiscard]] size_t RetainedPatchworkBytes() const noexcept {
    return Patchwork_ ? Patchwork_->HeapBytes() : 0u;
  }

  [[nodiscard]] size_t RetainedSurveyBytes() const noexcept {
    return Surveying_.Terrain.ProbePositionsM.capacity() * sizeof(Vec3f);
  }

  [[nodiscard]] bool AdvancesRetirement(size_t sheetsMost) noexcept {
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

  void SamplesProductPeak() noexcept { RecordsProductPeak(); }

  void HoldsCorridors(std::vector<EarthworkStamp> corridors) {
    CorridorHeapBytes_ = StampHeapBytes(corridors);
    Corridors_ = std::move(corridors);
  }

  [[nodiscard]] Generators::Corridors::Job *CorridorJob() noexcept { return CorridorJob_.get(); }

  void BeginsCorridors(std::unique_ptr<Generators::Corridors::Job> job) noexcept {
    CorridorJob_ = std::move(job);
  }

  [[nodiscard]] bool RetiringCorridors() const noexcept {
    return CorridorCompletion_ == CorridorCompletion::Retire;
  }

  void BeginsCorridorRetirement() noexcept {
    assert(CorridorCompletion_ == CorridorCompletion::Build);
    CorridorCompletion_ = CorridorCompletion::Retire;
  }

  void FinishesCorridors() noexcept { CorridorJob_.reset(); }

  [[nodiscard]] bool AdvancesCorridorRetirement(std::chrono::steady_clock::time_point began) {
    const auto retireAt = std::chrono::steady_clock::now();
    const bool retired = CorridorJob_->RetireStep(kCorridorRetireUnitsPerFrame);
    if (retired) { FinishesCorridors(); }
    SamplesCorridorRetirement(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - retireAt)
            .count());
    SamplesCorridorSlice(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
            .count());
    return retired;
  }

  [[nodiscard]] StreetGraphPreparation *StreetGraphWorker() noexcept {
    return StreetGraphWorker_.get();
  }

  void BeginStreetGraph(std::unique_ptr<StreetGraphPreparation> worker) noexcept {
    StreetGraphWorker_ = std::move(worker);
  }

  void FinishStreetGraph() noexcept { StreetGraphWorker_.reset(); }

  [[nodiscard]] Generators::TerrainRefinementJob *RefinementJob() noexcept {
    return RefinementJob_.get();
  }

  void BeginsRefinement(std::unique_ptr<Generators::TerrainRefinementJob> job) noexcept {
    RefinementJob_ = std::move(job);
  }

  void FinishesRefinement() noexcept { RefinementJob_.reset(); }

  [[nodiscard]] HeightSheets::HaloBuildJob *HaloJob() noexcept { return HaloJob_.get(); }

  void BeginsHalos(std::unique_ptr<HeightSheets::HaloBuildJob> job) noexcept {
    HaloJob_ = std::move(job);
  }

  void FinishesHalos() noexcept { HaloJob_.reset(); }

  void SamplesHaloSlice(double milliseconds) noexcept {
    LongestHaloSliceMs_ = std::max(LongestHaloSliceMs_, milliseconds);
  }

  [[nodiscard]] double LongestHaloSliceMs() const noexcept { return LongestHaloSliceMs_; }

  void SamplesCorridorSlice(double milliseconds) noexcept {
    LongestCorridorSliceMs_ = std::max(LongestCorridorSliceMs_, milliseconds);
  }

  [[nodiscard]] double LongestCorridorSliceMs() const noexcept { return LongestCorridorSliceMs_; }

  void SamplesCorridorRetirement(double milliseconds) noexcept {
    LongestCorridorRetirementMs_ = std::max(LongestCorridorRetirementMs_, milliseconds);
  }

  [[nodiscard]] double LongestCorridorRetirementMs() const noexcept {
    return LongestCorridorRetirementMs_;
  }

  void SamplesGeometrySlice(double milliseconds) noexcept {
    LongestGeometrySliceMs_ = std::max(LongestGeometrySliceMs_, milliseconds);
  }

  [[nodiscard]] double LongestGeometrySliceMs() const noexcept { return LongestGeometrySliceMs_; }

  [[nodiscard]] GeometrySubmission GeometrySubmissionStep() const noexcept {
    return GeometrySubmission_;
  }

  void ClassesStarted() noexcept {
    assert(GeometrySubmission_ == GeometrySubmission::Classes);
    GeometrySubmission_ = GeometrySubmission::ClassRanges;
  }

  void ClassesUploaded() noexcept {
    assert(GeometrySubmission_ == GeometrySubmission::ClassRanges);
    GeometrySubmission_ = GeometrySubmission::Begin;
  }

  void SamplesClassUploadSlice(double milliseconds) noexcept {
    LongestClassUploadMs_ = std::max(LongestClassUploadMs_, milliseconds);
    TotalClassUploadMs_ += milliseconds;
  }

  [[nodiscard]] double LongestClassUploadMs() const noexcept { return LongestClassUploadMs_; }

  [[nodiscard]] double TotalClassUploadMs() const noexcept { return TotalClassUploadMs_; }

  void SamplesClassComponents(const Render::GroundClassUploadMetrics &seen) noexcept {
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

  [[nodiscard]] const Render::GroundClassUploadMetrics &LongestClassComponents() const noexcept {
    return LongestClassComponents_;
  }

  void GeometryStarted() noexcept {
    assert(GeometrySubmission_ == GeometrySubmission::Begin);
    GeometrySubmission_ = GeometrySubmission::Cook;
  }

  [[nodiscard]] std::vector<EarthworkStamp> TakesCorridors() {
    CorridorHeapBytes_ = 0;
    return std::exchange(Corridors_, {});
  }

  [[nodiscard]] std::chrono::steady_clock::time_point Began() const noexcept { return Began_; }

  [[nodiscard]] bool Prepared() const noexcept { return Schedule_.Prepared(); }

  void MarkPrepared() noexcept {
    [[maybe_unused]] const bool prepared = Schedule_.MarkPrepared();
    assert(prepared);
  }

  [[nodiscard]] std::string_view Status() const noexcept {
    if (!Schedule_.Prepared()) { return "candidate"; }
    if (!Patchwork_) { return "patchwork"; }
    return Schedule_.Status();
  }

  [[nodiscard]] uint8_t Progress() const noexcept {
    if (!Schedule_.Prepared()) { return 0; }
    if (!Patchwork_) { return 1; }
    if (Schedule_.CurrentSheetPhase() != Core::GroundBuildSchedule::SheetPhase::Ready) {
      return static_cast<uint8_t>(Schedule_.CurrentSheetPhase()) + 2u;
    }
    return static_cast<uint8_t>(Schedule_.CurrentStage()) + 6u;
  }

private:
  [[nodiscard]] static size_t StampHeapBytes(std::span<const EarthworkStamp> stamps) noexcept {
    size_t bytes = 0;
    for (const auto &stamp : stamps) { bytes += stamp.HeapBytes(); }
    return bytes;
  }

  [[nodiscard]] size_t CurrentProductBytes() const noexcept {
    const size_t phaseBytes = (Patchwork_ ? Patchwork_->HeapBytes() : 0u) +
                              RoadHeightCoverage_.Tiles.capacity() * sizeof(Data::TileId) +
                              RoadHeightCoverage_.SelectedRouteIndices.capacity() * sizeof(size_t) +
                              Corridors_.capacity() * sizeof(EarthworkStamp) +
                              Surveying_.Terrain.ProbePositionsM.capacity() * sizeof(Vec3f) +
                              (Stamping_ ? Stamping_->HeapBytes() : 0u) +
                              (Pressing_ ? Pressing_->HeapBytes() : 0u);
    const size_t candidateBytes =
        Stamping_ ? StampingCandidateHeapBytes_ : Candidate_.OwnedHeapBytes();
    return candidateBytes + phaseBytes + CorridorHeapBytes_ + RoadEarthworkHeapBytes_ +
           RoadEarthworks_.capacity() * sizeof(EarthworkStamp);
  }

  void RecordsProductPeak() noexcept {
    ProductPeakBytes_ = std::max(ProductPeakBytes_, CurrentProductBytes());
  }

  Around Coverage_;
  GroundRevision Revision_;
  GroundWorldCandidate Candidate_;
  std::vector<std::shared_ptr<const Generators::Osm::SourceSnapshot>> OriginalSources_;
  std::vector<std::shared_ptr<const Generators::Osm::StructureCell>> OriginalCells_;
  std::shared_ptr<const World::TransportNetworkSnapshot> TransportSnapshot_;
  RoadHeightCoverage RoadHeightCoverage_;
  bool RoadAlignmentRequested_ = false;
  bool RoadSurfaceBuilt_ = false;
  size_t NextRoadSurfaceTransfer_ = 0;
  std::optional<Patchwork> Patchwork_;
  std::unique_ptr<Generators::BuildingStampJob> Stamping_;
  std::unique_ptr<Generators::TerrainDeformationTask> Pressing_;
  std::unique_ptr<Generators::Corridors::Job> CorridorJob_;
  std::unique_ptr<StreetGraphPreparation> StreetGraphWorker_;
  std::unique_ptr<GroundRegionPreparation> RegionWorker_;
  std::string RegionKey_;
  std::string RegionRequest_;
  RegionPhase RegionPhase_ = RegionPhase::Lookup;
  WaterPhase WaterPhase_ = WaterPhase::Awaiting;
  std::unique_ptr<WaterGeometryPreparation> WaterWorker_;
  std::unique_ptr<Generators::TerrainRefinementJob> RefinementJob_;
  std::unique_ptr<HeightSheets::HaloBuildJob> HaloJob_;
  std::vector<EarthworkStamp> Corridors_;
  std::vector<EarthworkStamp> RoadEarthworks_;
  ResidencyBuild Residency_;
  GroundSurvey Surveying_;
  std::chrono::steady_clock::time_point Began_ = std::chrono::steady_clock::now();
  size_t ProductPeakBytes_ = 0;
  size_t StampingCandidateHeapBytes_ = 0;
  size_t CorridorHeapBytes_ = 0;
  size_t RoadEarthworkHeapBytes_ = 0;
  double LongestPressingSliceMs_ = 0.0;
  double LongestCorridorSliceMs_ = 0.0;
  double LongestCorridorRetirementMs_ = 0.0;
  double LongestGeometrySliceMs_ = 0.0;
  double LongestClassUploadMs_ = 0.0;
  double TotalClassUploadMs_ = 0.0;
  Render::GroundClassUploadMetrics LongestClassComponents_;
  GeometrySubmission GeometrySubmission_ = GeometrySubmission::Classes;
  CorridorCompletion CorridorCompletion_ = CorridorCompletion::Build;
  double LongestHaloSliceMs_ = 0.0;
  Core::GroundBuildSchedule Schedule_;
  uint64_t Id_ = 0;
};

}
#endif
