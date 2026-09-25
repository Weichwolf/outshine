#include "GeodeticCamera.h"
#include "Digest.h"
#include "math/Units.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "Capacity.h"
#include "Log.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <expected>
#include <format>
#include <memory>
#include <cmath>
#include "Heap.h"
#include "TangentFrame.h"
#include <array>
#include <cassert>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <numbers>
#include <string>
#include <ratio>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <chrono>
#include <vector>

#include "Fit.h"
#include "ReferenceLine.h"

#include "spatial/Census.h"
#include "spatial/Drape.h"
#include "geo/PlaceKey.h"
#include "spatial/Refine.h"
#include "Corridors.h"
#include "BuildingStampJob.h"
#include "TerrainMesh.h"
#include "TerrainPress.h"
#include "EngineHeld.h"
#include "GroundWorldCandidate.h"
#include "GroundDiagnostics.h"
#include "WaterSurfaceBuilder.h"
#include "GroundBuildSchedule.h"
#include "GroundMesher.h"
#include "VectorStreetGraph.h"
#include "VectorStreetGraphWorker.h"
#include "RoadHeightCoverage.h"
#include "RoadRefinementCoverage.h"

namespace outshine {
namespace Says {
constexpr auto MaterialCreationFailed = "could not create ground materials";
constexpr auto CorridorTerrainIncomplete =
    "corridor generation reached terrain without a DEM field";
constexpr auto GroundContactIncomplete = "ground patchwork has no ready contact terrain";
}

constexpr uint64_t kLowWord = 0xFFFFFFFFULL;

static_assert(Ground::kStreamGrid == 2 * kPatchGrid,
              "the elevation stream is opened ONE zoom below the finest tile so its posting equals "
              "the patchwork's vertex spacing; that holds only while a stream tile carries twice "
              "the intervals a patchwork tile lays, and if either grid moves the zoom must be "
              "re-derived rather than kept");

namespace {

class ScopedCounter {
public:
  explicit ScopedCounter(Spent::Counter &counter) noexcept
      : Counter_(counter), Began_(std::chrono::steady_clock::now()) {}

  ScopedCounter(const ScopedCounter &) = delete;
  ScopedCounter &operator=(const ScopedCounter &) = delete;

  ~ScopedCounter() {
    Counter_.Took(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Began_)
            .count());
  }

private:
  Spent::Counter &Counter_;
  std::chrono::steady_clock::time_point Began_;
};

constexpr double kPerMille = 1000.0;
constexpr float kVerticalSlopeDeg = 90.0f;

constexpr float kWallRed = 0.74f;
constexpr float kWallGreen = 0.71f;
constexpr float kWallBlue = 0.65f;
constexpr float kWallRoughness = 0.88f;
constexpr float kTileRed = 0.42f;
constexpr float kTileGreen = 0.20f;
constexpr float kTileBlue = 0.14f;
constexpr float kTileRoughness = 0.72f;

constexpr double kWaterBedM = 2.0;
constexpr double kWaterBankM = kWaterBedM / kBatterRise;
constexpr size_t kBounceProbeStride = 16;
constexpr size_t kPlayableStructureCandidates = 1;
constexpr size_t kRefinedStructureCandidates = 4;
constexpr size_t kTerrainSheetsPerFrame = 48;
constexpr size_t kTerrainResidencySheetsPerFrame = 64;
constexpr size_t kEarthworkSheetsPerFrame = 32;
constexpr size_t kEarthworkPointsPerFrame = 8192;
constexpr size_t kEarthworkStampUnitsPerFrame = 2048;
constexpr size_t kCorridorLanesPerFrame = 128;
constexpr size_t kCorridorNodesPerFrame = 64;
constexpr size_t kCorridorRetireUnitsPerFrame = 512;
constexpr size_t kShapeCookItemsPerFrame = 262144;
constexpr size_t kClassUploadBytesPerFrame = 1u << 20u;
constexpr size_t kHaloNodesPerFrame = 32768;
constexpr size_t kTerrainRefinementSourcesPerFrame = 8;
constexpr size_t kGroundRestorePagesPerFrame = 64;
constexpr size_t kPieceRestorePerFrame = 4;

uint64_t DigestPatchwork(const Patchwork &patchwork) {
  uint64_t digest = kDigestBasis;
  const auto fold = [&digest](uint32_t word) { digest = (digest ^ word) * kDigestPrime; };
  for (const Sheet &sheet : patchwork.Sheets) {
    fold(static_cast<uint32_t>(sheet.Tile.Zoom));
    fold(sheet.Tile.X);
    fold(sheet.Tile.Y);
    fold(static_cast<uint32_t>(sheet.Side));
    fold(sheet.Postings);
    fold(static_cast<uint32_t>(sheet.Virtual));
    fold(static_cast<uint32_t>(sheet.SourceZoom));
    for (const float node : sheet.Nodes) { fold(std::bit_cast<uint32_t>(node)); }
  }
  return digest;
}

uint64_t DigestEarthworks(std::span<const EarthworkStamp> earthworks) {
  uint64_t digest = kDigestBasis;
  const auto fold = [&digest](uint32_t word) { digest = (digest ^ word) * kDigestPrime; };
  const auto foldDouble = [&fold](double value) {
    const auto bits = std::bit_cast<uint64_t>(value);
    fold(static_cast<uint32_t>(bits));
    fold(static_cast<uint32_t>(bits >> 32U));
  };
  for (const EarthworkStamp &one : earthworks) {
    fold(static_cast<uint32_t>(one.RingEastNorthM.size()));
    for (const double value : one.RingEastNorthM) { foldDouble(value); }
    fold(static_cast<uint32_t>(one.HoleRingsEastNorthM.size()));
    for (const auto &hole : one.HoleRingsEastNorthM) {
      fold(static_cast<uint32_t>(hole.size()));
      for (const double value : hole) { foldDouble(value); }
    }
    foldDouble(one.LowE);
    foldDouble(one.HighE);
    foldDouble(one.LowN);
    foldDouble(one.HighN);
    foldDouble(one.AtE);
    foldDouble(one.AtN);
    foldDouble(one.PlateauM);
    foldDouble(one.SlopeE);
    foldDouble(one.SlopeN);
    fold(static_cast<uint32_t>(one.SeamEastNorthM.size()));
    for (const double value : one.SeamEastNorthM) { foldDouble(value); }
    fold(static_cast<uint32_t>(one.Profile.has_value()));
    if (one.Profile) {
      const ProfiledCorridorSpan profile = one.Profile.value_or(ProfiledCorridorSpan{});
      fold(static_cast<uint32_t>(profile.CorridorKey));
      fold(static_cast<uint32_t>(profile.CorridorKey >> 32u));
      foldDouble(profile.BeginM.EastM);
      foldDouble(profile.BeginM.NorthM);
      foldDouble(profile.EndM.EastM);
      foldDouble(profile.EndM.NorthM);
      foldDouble(profile.BeginDerivativeM.EastM);
      foldDouble(profile.BeginDerivativeM.NorthM);
      foldDouble(profile.BeginDerivativeM.UpM);
      foldDouble(profile.EndDerivativeM.EastM);
      foldDouble(profile.EndDerivativeM.NorthM);
      foldDouble(profile.EndDerivativeM.UpM);
      foldDouble(profile.StationLengthM);
      foldDouble(profile.BeginBedM);
      foldDouble(profile.EndBedM);
      foldDouble(profile.BeginPavementHalfWidthM);
      foldDouble(profile.EndPavementHalfWidthM);
      foldDouble(profile.BeginHalfWidthM);
      foldDouble(profile.EndHalfWidthM);
    }
    foldDouble(one.ApronM);
    foldDouble(one.YieldM);
    foldDouble(one.SagInv);
    fold(static_cast<uint32_t>(one.Fills));
    fold(static_cast<uint32_t>(one.Kind));
  }
  return digest;
}

bool GroundSourcesReady(const Ground::GroundStack &stack, GroundQuality quality) {
  return quality == GroundQuality::Refined ? stack.Ingested() : stack.IngestedWithin(0);
}

}

class GroundBuildState {
public:
  enum class GeometrySubmission : uint8_t { Classes, ClassRanges, Begin, Cook };
  enum class CorridorCompletion : uint8_t { Build, Retire };

  struct MeshBuild {
    Generators::TerrainMesh Mesh;
    size_t NextSheet = 0;
    bool SheetsStitched = false;
    bool ResidencyStarted = false;
    bool ResidencyReady = false;
    double LongestSliceMs = 0.0;
    double LongestResidencySliceMs = 0.0;
  };

  GroundBuildState(Render::SceneRenderer &renderer,
                   const Surrounds &world,
                   const Ground::BuildingField &footprints,
                   Around coverage,
                   GroundRevision revision,
                   uint64_t id)
      : Coverage_(coverage),
        Revision_(revision),
        Candidate_(renderer, world, footprints),
        TransportSnapshot_(world.OsmTransportLoader && world.OsmTransportLoader->CurrentPhase() ==
                                                           World::OsmTransportLoader::Phase::Ready
                               ? world.OsmTransportLoader->Current()
                               : nullptr),
        Id_(id) {}

  [[nodiscard]] bool Matches(const GroundRevision &revision) const noexcept {
    return Revision_.Region == revision.Region && Revision_.Classes == revision.Classes &&
           Revision_.Footprints == revision.Footprints &&
           Revision_.VectorGeneration == revision.VectorGeneration &&
           Revision_.TransportSourceGeneration == revision.TransportSourceGeneration &&
           Revision_.StreetTiles == revision.StreetTiles &&
           Revision_.WaterTiles == revision.WaterTiles &&
           Revision_.Projection == revision.Projection && Revision_.Coverage == revision.Coverage &&
           Revision_.Quality == revision.Quality;
  }

  [[nodiscard]] uint32_t RevisionDifference(const GroundRevision &revision) const noexcept {
    uint32_t difference = 0;
    difference |= Revision_.Region != revision.Region ? 1u << 0u : 0u;
    difference |= Revision_.Classes != revision.Classes ? 1u << 1u : 0u;
    difference |= Revision_.Footprints != revision.Footprints ? 1u << 2u : 0u;
    difference |= Revision_.StreetTiles != revision.StreetTiles ? 1u << 3u : 0u;
    difference |= Revision_.WaterTiles != revision.WaterTiles ? 1u << 4u : 0u;
    difference |= Revision_.Projection != revision.Projection ? 1u << 5u : 0u;
    difference |= Revision_.Coverage != revision.Coverage ? 1u << 6u : 0u;
    difference |= Revision_.Quality != revision.Quality ? 1u << 7u : 0u;
    difference |= Revision_.VectorGeneration != revision.VectorGeneration ? 1u << 8u : 0u;
    difference |=
        Revision_.TransportSourceGeneration != revision.TransportSourceGeneration ? 1u << 9u : 0u;
    return difference;
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
    RoadEarthworks_ = std::move(stamps);
  }

  [[nodiscard]] std::vector<EarthworkStamp> TakeRoadEarthworks() noexcept {
    return std::move(RoadEarthworks_);
  }

  [[nodiscard]] MeshBuild &Meshing() noexcept { return Meshing_; }

  [[nodiscard]] MeshBuild &InitialMeshing() noexcept { return InitialMeshing_; }

  [[nodiscard]] Generators::TerrainPressJob *Pressing() noexcept { return Pressing_.get(); }

  [[nodiscard]] Generators::BuildingStampJob *Stamping() noexcept { return Stamping_.get(); }

  void BeginsStamping(std::unique_ptr<Generators::BuildingStampJob> stamping) noexcept {
    Stamping_ = std::move(stamping);
  }

  void FinishesStamping() noexcept { Stamping_.reset(); }

  void BeginsPressing(std::unique_ptr<Generators::TerrainPressJob> pressing) noexcept {
    Pressing_ = std::move(pressing);
  }

  void FinishesPressing() noexcept { Pressing_.reset(); }

  void SamplesPressingSlice(double milliseconds) noexcept {
    LongestPressingSliceMs_ = std::max(LongestPressingSliceMs_, milliseconds);
  }

  [[nodiscard]] double LongestPressingSliceMs() const noexcept { return LongestPressingSliceMs_; }

  [[nodiscard]] Ground::BuildingField &Footprints() noexcept {
    return Candidate_.Products().Footprints;
  }

  void PublishesFootprints() noexcept { Revision_.Footprints = Footprints().Revision(); }

  [[nodiscard]] Patchwork *Laid() noexcept { return Patchwork_ ? &*Patchwork_ : nullptr; }

  void Lays(Patchwork patchwork) noexcept { Patchwork_.emplace(std::move(patchwork)); }

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
    return Candidate_.Products().OwnedHeapBytes();
  }

  [[nodiscard]] size_t RetainedPatchworkBytes() const noexcept {
    return Patchwork_ ? Patchwork_->HeapBytes() : 0u;
  }

  [[nodiscard]] size_t RetainedMeshBytes() const noexcept {
    return Meshing_.Mesh.PositionsM.capacity() * sizeof(float) +
           Meshing_.Mesh.Indices.capacity() * sizeof(uint32_t) +
           InitialMeshing_.Mesh.PositionsM.capacity() * sizeof(float) +
           InitialMeshing_.Mesh.Indices.capacity() * sizeof(uint32_t);
  }

  [[nodiscard]] bool AdvancesRetirement(size_t sheetsMost) noexcept {
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

  void HoldsCorridors(std::vector<EarthworkStamp> corridors) { Corridors_ = std::move(corridors); }

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

  [[nodiscard]] VectorStreetGraphWorker *StreetGraphWorker() noexcept {
    return StreetGraphWorker_.get();
  }

  void BeginStreetGraph(std::unique_ptr<VectorStreetGraphWorker> worker) noexcept {
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

  [[nodiscard]] std::vector<EarthworkStamp> TakesCorridors() { return std::move(Corridors_); }

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
  [[nodiscard]] size_t CurrentProductBytes() const noexcept {
    const size_t phaseBytes = (Patchwork_ ? Patchwork_->HeapBytes() : 0u) +
                              RoadHeightCoverage_.Tiles.capacity() * sizeof(Data::TileId) +
                              RoadHeightCoverage_.SelectedRouteIndices.capacity() * sizeof(size_t) +
                              Corridors_.capacity() * sizeof(EarthworkStamp) +
                              Meshing_.Mesh.PositionsM.capacity() * sizeof(float) +
                              Meshing_.Mesh.Indices.capacity() * sizeof(uint32_t) +
                              InitialMeshing_.Mesh.PositionsM.capacity() * sizeof(float) +
                              InitialMeshing_.Mesh.Indices.capacity() * sizeof(uint32_t) +
                              (Stamping_ ? Stamping_->HeapBytes() : 0u) +
                              (Pressing_ ? Pressing_->HeapBytes() : 0u);
    size_t corridorBytes = 0;
    for (const EarthworkStamp &corridor : Corridors_) { corridorBytes += corridor.HeapBytes(); }
    size_t roadEarthworkBytes = RoadEarthworks_.capacity() * sizeof(EarthworkStamp);
    for (const EarthworkStamp &stamp : RoadEarthworks_) { roadEarthworkBytes += stamp.HeapBytes(); }
    return Candidate_.Products().OwnedHeapBytes() + phaseBytes + corridorBytes + roadEarthworkBytes;
  }

  void RecordsProductPeak() noexcept {
    ProductPeakBytes_ = std::max(ProductPeakBytes_, CurrentProductBytes());
  }

  Around Coverage_;
  GroundRevision Revision_;
  GroundWorldCandidate Candidate_;
  std::shared_ptr<const World::TransportNetworkSnapshot> TransportSnapshot_;
  RoadHeightCoverage RoadHeightCoverage_;
  bool RoadAlignmentRequested_ = false;
  bool RoadSurfaceBuilt_ = false;
  size_t NextRoadSurfaceTransfer_ = 0;
  std::optional<Patchwork> Patchwork_;
  std::unique_ptr<Generators::BuildingStampJob> Stamping_;
  std::unique_ptr<Generators::TerrainPressJob> Pressing_;
  std::unique_ptr<Generators::Corridors::Job> CorridorJob_;
  std::unique_ptr<VectorStreetGraphWorker> StreetGraphWorker_;
  std::unique_ptr<Generators::TerrainRefinementJob> RefinementJob_;
  std::unique_ptr<HeightSheets::HaloBuildJob> HaloJob_;
  std::vector<EarthworkStamp> Corridors_;
  std::vector<EarthworkStamp> RoadEarthworks_;
  MeshBuild Meshing_;
  MeshBuild InitialMeshing_;
  std::chrono::steady_clock::time_point Began_ = std::chrono::steady_clock::now();
  size_t ProductPeakBytes_ = 0;
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

Surrounds::Surrounds() = default;

Surrounds::~Surrounds() = default;

std::vector<float> Engine::State::PaletteOver(const Ground::VegetationTemplates &wearing,
                                              const Medium &fallback) {
  const size_t rows = wearing.TemplateCount();
  constexpr size_t kSurfaceStride = 8u;
  const size_t surfacesAt = kPaletteStride * (rows + 2u) + rows + 1u;
  std::vector<float> palette(surfacesAt + kSurfaceStride * (rows + 1u), 0.0f);
  palette[0] = std::bit_cast<float>(static_cast<uint32_t>(rows));
  palette[1] = std::bit_cast<float>(static_cast<uint32_t>(wearing.RockTemplate()));
  palette[2] = wearing.Limit().SlopeBandDeg();
  const auto rowAt = [](size_t row) { return kPaletteStride * (row + 1u); };
  for (size_t row = 0; row < rows; ++row) {
    for (size_t channel = 0; channel < kPaletteStride; ++channel) {
      palette[rowAt(row) + channel] = wearing.Rows()[row].Ground[channel];
    }
    palette[kPaletteStride * (rows + 2u) + row] = wearing.Rows()[row].Edge[3];
    for (size_t channel = 0; channel < kPaletteStride; ++channel) {
      palette[surfacesAt + kSurfaceStride * row + channel] =
          wearing.Rows()[row].GroundSurf[channel];
    }
    palette[surfacesAt + kSurfaceStride * row + kPaletteStride] = wearing.Rows()[row].Mix[1];
  }
  for (size_t channel = 0; channel < 3; ++channel) {
    palette[rowAt(rows) + channel] = fallback.GroundAlbedo[channel];
  }
  palette[rowAt(rows) + 3u] = Material{}.Roughness;
  palette[kPaletteStride * (rows + 2u) + rows] = kVerticalSlopeDeg;
  return palette;
}

Engine::State::Classed Engine::State::Classify(std::span<const float> groundPositionsM,
                                               GroundWorldCandidate &candidate) {
  Classed out;
  const std::shared_ptr<const ClassStructure> classes = World.Stack.Classes().Read();
  const Ground::VegetationTemplates &wearing = World.Stack.Vegetation();
  const Medium fallback = kEarthAir;
  if (classes && wearing.Ready()) {
    out.Structure = classes;
    out.Palette = PaletteOver(wearing, fallback);
  }
  if (out.Structure && !out.Palette.empty()) {
    const size_t rows = std::bit_cast<uint32_t>(out.Palette[0]);
    Vec3 wornSum = {{0.0, 0.0, 0.0}};
    double worn = 0.0;
    for (size_t at = 0; at + 2 < groundPositionsM.size(); at += 3u * kBounceProbeStride) {
      const int which = out.Structure->Evaluate(static_cast<double>(groundPositionsM[at]),
                                                -static_cast<double>(groundPositionsM[at + 2]),
                                                nullptr,
                                                nullptr);
      const size_t row =
          which >= 0 && std::cmp_less(which, rows) ? static_cast<size_t>(which) : rows;
      for (int channel = 0; channel < 3; ++channel) {
        wornSum[channel] += static_cast<double>(
            out.Palette[kPaletteStride + row * kPaletteStride + static_cast<size_t>(channel)]);
      }
      worn += 1.0;
    }
    if (worn > 0.0) {
      const Vec3 wornMean = {{wornSum[0] / worn, wornSum[1] / worn, wornSum[2] / worn}};
      candidate.Grounding(wornMean);
      Published.Places(
          "lighting: the ground it bounces off, red", kPerMille * wornMean[0], "albedo/1000");
      Published.Places("lighting: green", kPerMille * wornMean[1], "albedo/1000");
      Published.Places("lighting: blue", kPerMille * wornMean[2], "albedo/1000");
    }
  }
  const Render::SubjectEnvironment &lighting = candidate.AmbientStanding();
  Published.Places("lighting: the sky's own radiance, red", lighting.RadianceLinear[0], "cd/m2");
  Published.Places("lighting: sky green", lighting.RadianceLinear[1], "cd/m2");
  Published.Places("lighting: sky blue", lighting.RadianceLinear[2], "cd/m2");
  Published.Places(
      "lighting: the ground's bounced radiance, red", lighting.GroundLinear[0], "cd/m2");
  Published.Places("lighting: bounce green", lighting.GroundLinear[1], "cd/m2");
  Published.Places("lighting: bounce blue", lighting.GroundLinear[2], "cd/m2");
  Published.Places(
      "class field: the vegetation table is ready", wearing.Ready() ? 1.0 : 0.0, "yes/no");
  Published.Places(
      "class field: rows the table carries", static_cast<double>(wearing.TemplateCount()), "rows");
  Published.Places("class field: features the fine tier holds",
                   static_cast<double>(World.Stack.Classes().FeaturesHeld()),
                   "features");
  Published.Places("class field: of those it has taken",
                   static_cast<double>(World.Stack.Classes().FeaturesTaken()),
                   "features");
  Published.Places("class field: it published a structure", classes ? 1.0 : 0.0, "yes/no");
  Published.Places("class field: the version the colours used",
                   classes ? static_cast<double>(classes->Version()) : -1.0,
                   "version");
  if (classes) {
    uint64_t digest = kDigestBasis;
    const size_t words = classes->Bytes() / sizeof(uint32_t);
    for (size_t at = 0; at < words; ++at) {
      digest = (digest ^ classes->Words()[at]) * kDigestPrime;
    }
    Published.Places("class field: the structure's digest, low half",
                     static_cast<double>(digest & kLowWord),
                     "digest");
    Published.Places("class field: the structure's digest, high half",
                     static_cast<double>(digest >> 32U),
                     "digest");
  }
  Published.Places("class field: it calls itself complete",
                   World.Stack.Classes().Complete() ? 1.0 : 0.0,
                   "yes/no");
  Published.Places("class field: tiles it waits for",
                   static_cast<double>(World.Stack.Classes().PendingTiles()),
                   "tiles");
  Published.Places("class field: the fraction it has no data for",
                   classes ? classes->NoDataFraction() : -1.0,
                   "fraction");
  Published.Places("class field: the materials are loaded", wearing.Ready() ? 1.0 : 0.0, "yes/no");
  return out;
}

bool Engine::State::PrepareBuildingSurfaces(const TangentFrame &standing,
                                            GroundBuildProducts &build,
                                            Phasing &clocks) {
  Core::ReportBuildingFootprints(
      Published, World.Stack.Footprints(), World.Stack.Vectors(), standing);
  if (!build.Surfaces) {
    Geometry materials;
    Material walls;
    walls.BaseColour[0] = kWallRed;
    walls.BaseColour[1] = kWallGreen;
    walls.BaseColour[2] = kWallBlue;
    walls.Roughness = kWallRoughness;
    walls.Pattern = SurfacePattern::Facade;
    Material tiles;
    tiles.BaseColour[0] = kTileRed;
    tiles.BaseColour[1] = kTileGreen;
    tiles.BaseColour[2] = kTileBlue;
    tiles.Roughness = kTileRoughness;
    if (!materials.addSurface("walls", walls) || !materials.addSurface("roofs", tiles)) {
      Error = Says::MaterialCreationFailed;
      return false;
    }
    auto first = Picture.Device.RegisterPieceMaterials(std::move(materials));
    if (!first) {
      Error = std::move(first.error());
      return false;
    }
    build.Surfaces = TilePieces::Surfaces{.Walls = Render::PieceSurface::Registered(*first),
                                          .Roofs = Render::PieceSurface::Registered(*first + 1u)};
  }
  Published.Places(
      "rebuild: the ground ring took",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - clocks.PhaseAt)
          .count(),
      "ms");
  clocks.PhaseAt = std::chrono::steady_clock::now();
  clocks.CensusAt = clocks.PhaseAt;
  Published.Places(
      "buildings: the wall surface", static_cast<double>(build.Surfaces->Walls.Index), "index");
  Published.Places(
      "buildings: the roof surface", static_cast<double>(build.Surfaces->Roofs.Index), "index");
  Published.Places("buildings: tiles handed to the arena as pieces",
                   static_cast<double>(World.Pieces.Handed()),
                   "tiles");
  Published.Places(
      "buildings: pieces the arena refused", static_cast<double>(World.Pieces.Refused()), "pieces");
  Published.Places("buildings: triangles the tiles handed over",
                   static_cast<double>(World.Stack.Footprints().TrianglesHanded()),
                   "triangles");
  if (Picture.Standing) {
    Published.Places("buildings: pieces standing in the arena",
                     static_cast<double>(Picture.Device.PiecesStanding()),
                     "pieces");
    Published.Places("buildings: triangles the arena holds",
                     static_cast<double>(Picture.Device.PieceTriangles()),
                     "triangles");
    Published.Places("buildings: bytes the arena holds on the device",
                     static_cast<double>(Picture.Device.PieceBytesHeld()),
                     "bytes");
  }
  return true;
}

Engine::State::Laid Engine::State::Focuses(GroundRequest &request,
                                           LongitudeLatitude at,
                                           bool alsoWhenTilesLanded,
                                           GroundQuality quality) {
  const Around &over = request.Coverage;
  const GroundRevision previous = World.GroundPublished.Current().value_or(GroundRevision{});
  const double atLat = at.LatitudeDeg;
  const double atLon = at.LongitudeDeg;
  const Ground::TileFrac here = Ground::ToTileFracClamped(
      Ground::Geo{.LongitudeDeg = atLon, .LatitudeDeg = atLat}, over.Zoom);
  const uint64_t from = (static_cast<uint64_t>(static_cast<int64_t>(std::floor(here.X))) << 32U) ^
                        static_cast<uint64_t>(static_cast<int64_t>(std::floor(here.Y))) ^
                        (static_cast<uint64_t>(over.Levels) << 56u);
  World.Stack.Pool().Focus({.LongitudeDeg = atLon, .LatitudeDeg = atLat});
  ++World.Asked;
  Around asking = over;
  asking.Asking = true;
  asking.PlayableOnly = quality == GroundQuality::Playable;
  auto sees = World.Shipping.Covering().Lay(World.Stack.Pool(), asking);
  if (!sees) {
    Error = sees.error();
    return Laid::Refused;
  }
  World.AskedPending = sees->Pending;
  World.AskedWanted = sees->Tiles;
  if (asking.PlayableOnly) { World.AskedPlayablePending = sees->Pending; }
  const size_t resident = sees->Tiles > sees->Pending ? sees->Tiles - sees->Pending : 0;
  const std::shared_ptr<const ClassStructure> naming = World.Stack.Classes().Read();
  const uint64_t classes = naming ? naming->Version() : 0;
  const bool elsewhere = from != previous.Region;
  const bool renamed = classes != previous.Classes;
  const uint64_t footprints = World.Stack.Footprints().Revision();
  const World::OsmTransportLoader *const transportLoader = World.OsmTransportLoader.get();
  const uint64_t transportGeneration =
      transportLoader != nullptr &&
              transportLoader->CurrentPhase() == World::OsmTransportLoader::Phase::Ready &&
              transportLoader->Current()
          ? transportLoader->CompletedCount()
          : 0;
  const Render::Viewpoint &view = Picture.Standing->Watching();
  const std::array<double, 3> projection{
      {static_cast<double>(view.Kind), view.YfovRad, view.YMagM}};
  const double visualRadiusM = Session.Declared.Ground.SightM > 0.0 ? Session.Declared.Ground.SightM
                                                                    : Scenario::kSightUnsaidM;
  request.Revision = {.Region = from,
                      .ResidentTiles = resident,
                      .Classes = classes,
                      .Footprints = footprints,
                      .VectorGeneration = World.Stack.Vectors() != nullptr
                                              ? World.Stack.Vectors()->Generation()
                                              : 0,
                      .TransportSourceGeneration = transportGeneration,
                      .StreetTiles = World.Stack.Ways().IngestedTiles(),
                      .WaterTiles = World.Stack.WaterBodies().IngestedTiles(),
                      .Projection = projection,
                      .Coverage = {.ContactRadiusM = 0.5 * World.Stack.Footprints().TileSpanM(),
                                   .VisualRadiusM = visualRadiusM,
                                   .MinimumSourceZoom = std::max(over.Zoom - 1, 0),
                                   .TargetSourceZoom = over.Zoom},
                      .Quality = quality};
  if (quality == GroundQuality::Refined) { World.RequestedRefinedGround = request.Revision; }
  Published.Places("building triangles the world meshed",
                   static_cast<double>(World.Stack.Footprints().TrianglesHanded()),
                   "triangles");
  Published.Places(
      "world: the bytes its fields hold", static_cast<double>(World.Stack.HeapBytes()), "bytes");
  Published.Places("world: of that, the land classes",
                   static_cast<double>(World.Stack.Classes().HeapBytes()),
                   "bytes");
  Published.Places(
      "world: the buildings", static_cast<double>(World.Stack.Footprints().HeapBytes()), "bytes");
  Published.Places("world: of those, the footprints it keeps",
                   static_cast<double>(World.Stack.Footprints().PrintBytes()),
                   "bytes");
  Published.Places("world: tiles baking on the workers right now",
                   static_cast<double>(World.StructureBuilds.Queued()),
                   "tiles");
  Published.Places("world: the building pieces the device holds",
                   Picture.Standing ? static_cast<double>(Picture.Device.PieceBytesHeld()) : 0.0,
                   "bytes");
  Published.Places(
      "world: the water", static_cast<double>(World.Stack.WaterBodies().HeapBytes()), "bytes");
  Published.Places(
      "world: the streets", static_cast<double>(World.Stack.Ways().HeapBytes()), "bytes");
  Published.Places("world: the ceiling its fields stand under",
                   static_cast<double>(Ground::GroundStack::kHoldsBytes),
                   "bytes");
  Published.Places("world: times a round stopped at that ceiling",
                   static_cast<double>(World.Stack.OverCeiling()),
                   "rounds");
  Published.Places("world: and the OSM features",
                   World.Stack.Vectors() != nullptr
                       ? static_cast<double>(World.Stack.Vectors()->HeapBytes())
                       : 0.0,
                   "bytes");
  Published.Places("tiles laid bare on the ellipsoid",
                   static_cast<double>(sees->Pending + sees->Absent + sees->Refused),
                   "tiles");
  if (!World.GroundPublished.NeedsRebuild(
          request.Revision, alsoWhenTilesLanded, World.RimsMissing > 0)) {
    return Laid::Unchanged;
  }

  Published.Places(
      "rebuilds since the world stood", static_cast<double>(World.Relaid + 1u), "rebuilds");
  Published.Places("rebuild: the eye walked into another tile", elsewhere ? 1.0 : 0.0, "yes/no");
  Published.Places("rebuild: tiles resident when it did", static_cast<double>(resident), "tiles");
  Published.Places("rebuild: and resident the time before",
                   static_cast<double>(previous.ResidentTiles),
                   "tiles");
  Published.Places("rebuild: the land classes were named anew", renamed ? 1.0 : 0.0, "yes/no");
  return Laid::Wanted;
}

std::expected<Engine::State::GroundRequest, Engine::State::Laid>
Engine::State::RingWanted(bool alsoWhenTilesLanded, GroundQuality quality) {
  const Scenario::Document &declared = Session.Declared;
  const double anchorLat = declared.Ground.Origin.LatitudeDeg;
  const double anchorLon = declared.Ground.Origin.LongitudeDeg;
  const LongitudeLatitude eyeStands = CurrentGeographicFocus();
  const double atLat = eyeStands.LatitudeDeg;
  const double atLon = eyeStands.LongitudeDeg;
  Published.Places("the ring centres this far from the world's anchor",
                   std::hypot((atLat - anchorLat) * kMPerDegLat,
                              (atLon - anchorLon) * kMPerDegLon * std::cos(anchorLat * kDeg2Rad)),
                   "m");

  Around over;
  over.LatitudeDeg = atLat;
  over.LongitudeDeg = atLon;
  over.Zoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  {
    const double tileSpanM = 40075017.0 * std::cos(atLat * kDeg2Rad) / std::ldexp(1.0, over.Zoom);
    const double nearest = 4.0 * tileSpanM;
    const double wanted = declared.Ground.SightM > 0.0 ? declared.Ground.SightM : 240000.0;
    const double doublings = wanted > nearest ? std::log2(wanted / nearest) : 0.0;
    over.Levels = 1 + static_cast<int>(std::ceil(doublings));
    Published.Places("the sight a scenario declares", wanted, "m");
    Published.Places("and what one tile spans at the finest zoom", tileSpanM, "m");
    Published.Places("the elevation's own posting",
                     World.Stack.Ground().PostM(declared.Ground.Origin.LatitudeDeg),
                     "m");
    Published.Places("and the drawn mesh's vertex spacing",
                     over.Grid > 1 ? tileSpanM / static_cast<double>(over.Grid - 1) : 0.0,
                     "m");
  }
  if (Session.Views) {
    const Scenario::View &camera = Session.Views->Active();
    if (camera.Placement == Scenario::CameraPlacement::Geodetic &&
        camera.Geographic.SamplesHeight) {
      const auto position = ResolveGeodeticCamera(
          camera, {.LongitudeDeg = anchorLon, .LatitudeDeg = anchorLat}, &World.Stack.Ground());
      if (!position) {
        Error = position.error();
        return std::unexpected(Laid::Refused);
      }
      if (!*position) { return std::unexpected(Laid::Pending); }
    }
  }
  if (!UpdateActiveCamera()) { return std::unexpected(Laid::Refused); }
  GroundRequest request{.Coverage = over, .Revision = {}};
  switch (Focuses(
      request, {.LongitudeDeg = atLon, .LatitudeDeg = atLat}, alsoWhenTilesLanded, quality)) {
    case Laid::Refused: return std::unexpected(Laid::Refused);
    case Laid::Pending: return std::unexpected(Laid::Pending);
    case Laid::Unchanged: return std::unexpected(Laid::Unchanged);
    case Laid::Wanted: break;
  }
  return request;
}

namespace {
void AppendWaterBasinStamps(const Ground::WaterField &water,
                            std::span<const double> points,
                            const TangentFrame &standing,
                            std::vector<EarthworkStamp> &yielding) {
  std::vector<std::pair<uint32_t, EarthworkStamp>> ordered;
  ordered.reserve(water.Surfaces().size());
  for (const Ground::WaterField::Surface &lake : water.Surfaces()) {
    const Ground::WaterField::SurfaceRing &ring = water.RingsOf(lake).front();
    if ((static_cast<size_t>(ring.FirstPoint) + ring.PointCount) * 2u > points.size()) { continue; }
    EarthworkStamp made;
    made.RingEastNorthM.reserve(static_cast<size_t>(ring.PointCount) * 2u);
    made.LowE = kBeyondAnyCoordinate;
    made.HighE = -kBeyondAnyCoordinate;
    made.LowN = kBeyondAnyCoordinate;
    made.HighN = -kBeyondAnyCoordinate;
    std::vector<double> bedM;
    bedM.reserve(ring.PointCount);
    for (uint32_t step = 0; step < ring.PointCount; ++step) {
      const size_t at = (static_cast<size_t>(ring.FirstPoint) + step) * 2u;
      const EastNorthUp shore =
          standing.ToLocalPosition({.LongitudeDeg = points[at + 1],
                                    .LatitudeDeg = points[at],
                                    .HeightM = static_cast<double>(lake.LevelM) - kWaterBedM});
      made.RingEastNorthM.push_back(shore.EastM);
      made.RingEastNorthM.push_back(shore.NorthM);
      made.LowE = std::min(made.LowE, shore.EastM);
      made.HighE = std::max(made.HighE, shore.EastM);
      made.LowN = std::min(made.LowN, shore.NorthM);
      made.HighN = std::max(made.HighN, shore.NorthM);
      bedM.push_back(shore.UpM);
    }
    made.AtE = 0.5 * (made.LowE + made.HighE);
    made.AtN = 0.5 * (made.LowN + made.HighN);
    made.SagInv = 1.0 / kWgs84A;
    double plateau = 0.0;
    for (size_t corner = 0; corner < bedM.size(); ++corner) {
      const double dE = made.RingEastNorthM[corner * 2u] - made.AtE;
      const double dN = made.RingEastNorthM[corner * 2u + 1u] - made.AtN;
      plateau += bedM[corner] + 0.5 * (dE * dE + dN * dN) * made.SagInv;
    }
    made.PlateauM = plateau / static_cast<double>(bedM.size());
    made.ApronM = kWaterBankM;
    made.YieldM = kWaterBedM;
    made.Kind = EarthworkKind::Basin;
    made.SeamEastNorthM = made.RingEastNorthM;
    bool complete = true;
    for (const Ground::WaterField::SurfaceRing &hole : water.RingsOf(lake).subspan(1)) {
      if ((static_cast<size_t>(hole.FirstPoint) + hole.PointCount) * 2u > points.size()) {
        complete = false;
        break;
      }
      std::vector<double> boundary;
      boundary.reserve(static_cast<size_t>(hole.PointCount) * 2u);
      for (uint32_t step = 0; step < hole.PointCount; ++step) {
        const size_t at = (static_cast<size_t>(hole.FirstPoint) + step) * 2u;
        const EastNorthUp shore =
            standing.ToLocalPosition({.LongitudeDeg = points[at + 1],
                                      .LatitudeDeg = points[at],
                                      .HeightM = static_cast<double>(lake.LevelM) - kWaterBedM});
        boundary.push_back(shore.EastM);
        boundary.push_back(shore.NorthM);
      }
      made.HoleRingsEastNorthM.push_back(std::move(boundary));
    }
    if (!complete) { continue; }
    ordered.emplace_back(ring.FirstPoint, std::move(made));
  }
  std::ranges::sort(ordered, {}, [](const auto &one) { return one.first; });
  for (auto &entry : ordered) { yielding.push_back(std::move(entry.second)); }
}
}

Engine::State::GroundBuildProgress
Engine::State::BuildGroundBuildingStamps(const TangentFrame &standing,
                                         GroundBuildState &state,
                                         const Ground::OsmField &shapes,
                                         std::vector<EarthworkStamp> &yielding) {
  const auto sliceAt = std::chrono::steady_clock::now();
  if (state.Stamping() == nullptr) {
    state.BeginsStamping(std::make_unique<Generators::BuildingStampJob>(
        standing, state.Revision().VectorGeneration));
  }
  const auto advanced = state.Stamping()->Advance({.Footprints = state.Footprints().Footprints(),
                                                   .Points = shapes.Points(),
                                                   .VectorGeneration = shapes.Generation(),
                                                   .UnitsMost = kEarthworkStampUnitsPerFrame});
  if (!advanced) {
    Error = std::string(advanced.error());
    return GroundBuildProgress::Failed;
  }
  state.SamplesPressingSlice(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sliceAt)
          .count());
  state.SamplesProductPeak();
  if (!*advanced) { return GroundBuildProgress::Pending; }
  auto stamped = std::move(*state.Stamping()).Take();
  if (!stamped) {
    Error = std::string(stamped.error());
    return GroundBuildProgress::Failed;
  }
  yielding = std::move(*stamped);
  state.FinishesStamping();
  return GroundBuildProgress::Ready;
}

bool Engine::State::PressGroundEarthworks(const TangentFrame &standing,
                                          Patchwork &patchwork,
                                          GroundBuildState &state) {
  const auto sliceAt = std::chrono::steady_clock::now();
  if (state.Pressing() == nullptr) {
    const Ground::OsmField *const shapes = World.Stack.Vectors();
    std::vector<EarthworkStamp> yielding;
    if (shapes != nullptr) {
      if (shapes->Generation() != state.Revision().VectorGeneration) {
        World.GroundBuild.reset();
        return true;
      }
      const GroundBuildProgress stamped =
          BuildGroundBuildingStamps(standing, state, *shapes, yielding);
      if (stamped != GroundBuildProgress::Ready) { return stamped != GroundBuildProgress::Failed; }
    } else if (state.Stamping() != nullptr) {
      World.GroundBuild.reset();
      return true;
    }
    std::vector<EarthworkStamp> corridor = state.TakesCorridors();
    std::vector<EarthworkStamp> roadEarthworks = state.TakeRoadEarthworks();
    corridor.reserve(corridor.size() + roadEarthworks.size());
    corridor.insert(corridor.end(),
                    std::make_move_iterator(roadEarthworks.begin()),
                    std::make_move_iterator(roadEarthworks.end()));
    if (shapes != nullptr) {
      uint64_t tileOrder = kDigestBasis;
      for (const Ground::OsmField::Tile &tile : shapes->Tiles()) {
        tileOrder = (tileOrder ^ static_cast<uint32_t>(tile.X)) * kDigestPrime;
        tileOrder = (tileOrder ^ static_cast<uint32_t>(tile.Y)) * kDigestPrime;
      }
      Published.Places("ground candidate: OSM tile order, low half",
                       static_cast<double>(tileOrder & kLowWord),
                       "digest");
      Published.Places("ground candidate: OSM tile order, high half",
                       static_cast<double>(tileOrder >> 32U),
                       "digest");
    }
    const size_t builtPads = yielding.size();
    if (shapes != nullptr) {
      AppendWaterBasinStamps(World.Stack.WaterBodies(), shapes->Points(), standing, yielding);
    }
    const size_t builtLakes = yielding.size() - builtPads;
    if (Session.Declared.Render.Audits) {
      const uint64_t padStamps = DigestEarthworks(std::span(yielding).first(builtPads));
      Published.Places("ground candidate: pad stamps digest, low half",
                       static_cast<double>(padStamps & kLowWord),
                       "digest");
      Published.Places("ground candidate: pad stamps digest, high half",
                       static_cast<double>(padStamps >> 32U),
                       "digest");
      const uint64_t lakeStamps =
          DigestEarthworks(std::span(yielding).subspan(builtPads, builtLakes));
      Published.Places("ground candidate: lake stamps digest, low half",
                       static_cast<double>(lakeStamps & kLowWord),
                       "digest");
      Published.Places("ground candidate: lake stamps digest, high half",
                       static_cast<double>(lakeStamps >> 32U),
                       "digest");
    }
    Published.Places("ground: lakes that press it", static_cast<double>(builtLakes), "lakes");
    yielding.insert(yielding.end(),
                    std::make_move_iterator(corridor.begin()),
                    std::make_move_iterator(corridor.end()));
    if (Session.Declared.Render.Audits) {
      const uint64_t corridorStamps =
          DigestEarthworks(std::span(yielding).subspan(builtPads + builtLakes, corridor.size()));
      Published.Places("ground candidate: corridor stamps digest, low half",
                       static_cast<double>(corridorStamps & kLowWord),
                       "digest");
      Published.Places("ground candidate: corridor stamps digest, high half",
                       static_cast<double>(corridorStamps >> 32U),
                       "digest");
    }
    Published.Places("ground: pads that press it", static_cast<double>(builtPads), "pads");
    Published.Places("ground: corridor pieces that press it",
                     static_cast<double>(yielding.size() - builtPads - builtLakes),
                     "pieces");
    state.BeginsPressing(std::make_unique<Generators::TerrainPressJob>(
        std::move(yielding),
        patchwork,
        standing,
        Generators::TerrainPageLayout{.Side = Render::GroundLattice::kSide, .Halo = 1},
        kMostEarthworkM));
    state.SamplesPressingSlice(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sliceAt)
            .count());
    state.SamplesProductPeak();
    return true;
  }
  const bool completed =
      state.Pressing()->Advance(kEarthworkSheetsPerFrame, kEarthworkPointsPerFrame);
  state.SamplesPressingSlice(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sliceAt)
          .count());
  state.SamplesProductPeak();
  if (!completed) { return true; }
  const Generators::PressedTerrain pressed_ = state.Pressing()->Take();
  state.FinishesPressing();
  Published.Places("ground: pressing gather", pressed_.GatherMs, "ms");
  Published.Places("ground: pressing decide", pressed_.DecideMs, "ms");
  Published.Places("ground: pressing buckets", pressed_.BucketMs, "ms");
  Published.Places("ground: pressing reject", pressed_.RejectMs, "ms");
  Published.Places("ground: pressing apply", pressed_.ApplyMs, "ms");
  Published.Places("ground: pressing write", pressed_.WriteMs, "ms");
  Published.Places("ground: pressing floors", pressed_.FloorsMs, "ms");
  Published.Places("ground: longest gather slice", pressed_.LongestGatherMs, "ms");
  Published.Places("ground: longest decide slice", pressed_.LongestDecideMs, "ms");
  Published.Places("ground: longest reject slice", pressed_.LongestRejectMs, "ms");
  Published.Places("ground: longest initialize slice", pressed_.LongestInitializeMs, "ms");
  Published.Places("ground: longest apply slice", pressed_.LongestApplyMs, "ms");
  Published.Places("ground: longest write slice", pressed_.LongestWriteMs, "ms");
  Published.Places("ground: longest reproject slice", pressed_.LongestReprojectMs, "ms");
  Published.Places("ground: longest floors slice", pressed_.LongestFloorsMs, "ms");
  Published.Places(
      "ground: lattice nodes the stamps pressed", static_cast<double>(pressed_.Nodes), "nodes");
  Published.Places("ground: stamps refused as STRUCTURES, past the earthwork bound",
                   static_cast<double>(pressed_.Structures),
                   "yields");
  Published.Places("ground: nodes held where a stamp still asked past the bound",
                   static_cast<double>(pressed_.Held),
                   "nodes");
  Published.Places("ground: and the deepest it cut", pressed_.DeepestM, "m");
  Published.Places("ground: and the highest it filled", pressed_.RaisedM, "m");
  for (const auto &[what, floors] :
       {std::pair{"pads", &pressed_.Pads}, std::pair{"corridor pieces", &pressed_.Corridors}}) {
    Published.Places(std::format("ground: {} with a lattice node inside", what),
                     static_cast<double>(floors->Stamps),
                     "stamps");
    Published.Places(std::format("ground: {} no lattice node reaches", what),
                     static_cast<double>(floors->Unreached),
                     "stamps");
    Published.Places(std::format("ground: nodes inside those {}", what),
                     static_cast<double>(floors->Nodes),
                     "nodes");
    Published.Places(std::format("ground: of those {} nodes, another stamp decided", what),
                     static_cast<double>(floors->Contested),
                     "nodes");
    Published.Places(
        std::format("ground: nodes inside {} above their plane after the press, worst", what),
        floors->AboveM,
        "m");
    Published.Places(
        std::format("ground: nodes inside {} that fill, below it after the press, worst", what),
        floors->BelowM,
        "m");
    Published.Places(std::format("ground: nodes inside {} that do not fill, below it, worst", what),
                     floors->UnfilledM,
                     "m");
    Published.Places(std::format("ground: those {} nodes above it before the press, worst", what),
                     floors->WasAboveM,
                     "m");
    Published.Places(
        std::format("ground: those filling {} nodes below it before the press, worst", what),
        floors->WasBelowM,
        "m");
  }
  const double pressingMs =
      pressed_.GatherMs + pressed_.DecideMs + pressed_.WriteMs + pressed_.FloorsMs;
  Published.Places("ground: of that, pressing", pressingMs, "ms");
  Published.Places("ground candidate: earthworks", pressingMs, "ms");
  Published.Places(
      "ground candidate: longest earthwork slice", state.LongestPressingSliceMs(), "ms");
  state.AdvanceStage();
  return true;
}

bool Engine::State::BuildWaterSurfaces(const TangentFrame &standing,
                                       Geometry &ground,
                                       MaterialInstance ringSurface) {
  const auto waterAt = std::chrono::steady_clock::now();
  const Ground::WaterField &water = World.Stack.WaterBodies();
  const Ground::OsmField *const vectors = World.Stack.Vectors();
  const std::span<const double> points =
      vectors != nullptr ? vectors->Points() : std::span<const double>{};
  const auto built =
      Generators::AppendWaterSurfaceGeometry(ground, ringSurface, water, points, standing);
  if (!built) {
    Error = built.error();
    return false;
  }
  Published.Places(
      "water: of that, laying the surfaces",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - waterAt).count(),
      "ms");
  Published.Places("water: surfaces laid", static_cast<double>(built->Laid), "surfaces");
  Published.Places(
      "water: surfaces refused", static_cast<double>(built->RefusedTopology), "surfaces");
  Published.Places("water: surfaces refused by topology",
                   static_cast<double>(built->RefusedTopology),
                   "surfaces");
  Published.Places("water: triangles", static_cast<double>(built->Triangles), "triangles");
  if (built->Triangles > 0) { Published.Places("water: the geometry took it", 1.0, "yes/no"); }
  return true;
}

Engine::State::GroundBuildProgress
Engine::State::AdvanceGroundCandidatePreparation(const GroundRequest &request) {
  if (!World.GroundBuild || !World.GroundBuild->Matches(request.Revision)) {
    if (World.GroundBuild) {
      Published.Places("ground candidate: revision mismatch mask",
                       static_cast<double>(World.GroundBuild->RevisionDifference(request.Revision)),
                       "bits");
      if (World.GroundRetirement) { return GroundBuildProgress::Pending; }
      Published.Places("ground candidate: canceled CPU products",
                       static_cast<double>(World.GroundBuild->RetainedProductBytes()),
                       "bytes");
      World.GroundRetirement = std::move(World.GroundBuild);
      return GroundBuildProgress::Pending;
    }
    ++World.GroundCandidates;
    const auto createAt = std::chrono::steady_clock::now();
    World.GroundBuild = std::make_unique<GroundBuildState>(Picture.Device,
                                                           World,
                                                           World.Stack.Footprints(),
                                                           request.Coverage,
                                                           request.Revision,
                                                           World.GroundCandidates);
    if (const auto *source = World.GroundBuild->TransportSnapshot()) {
      auto routeTiles = RoadHeightCoverage::Select(
          *source, {.Zoom = request.Coverage.Zoom, .MaximumEdges = 512, .MaximumTiles = 256});
      if (!routeTiles) {
        Error = std::move(routeTiles.error());
        World.GroundBuild.reset();
        return GroundBuildProgress::Failed;
      }
      Published.Places("semantic road route tiles requested",
                       static_cast<double>(routeTiles->Tiles.size()),
                       "tiles");
      Published.Places("semantic road routes deferred by local tile budget",
                       static_cast<double>(routeTiles->DeferredRoutes),
                       "routes");
      World.GroundBuild->SetRoadHeightCoverage(std::move(*routeTiles));
    }
    Cost.GroundBuildCreate.Took(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - createAt)
            .count());
    if (request.Revision.Quality == GroundQuality::Refined) {
      World.GroundBuild->Footprints().BeginRefinement();
    }
    Published.Places(
        "ground candidate: starts", static_cast<double>(World.GroundCandidates), "candidates");
    return GroundBuildProgress::Pending;
  }
  GroundBuildState &state = *World.GroundBuild;
  Published.Places(
      "ground candidate: progress", static_cast<double>(state.Progress()), "stage index");
  if (state.Prepared()) { return GroundBuildProgress::Ready; }
  const auto prepareAt = std::chrono::steady_clock::now();
  auto prepared = state.Candidate().AdvancePreparation(
      *Picture.Standing,
      &Picture.Face,
      {.Pieces = kPieceRestorePerFrame, .HeightPages = kGroundRestorePagesPerFrame});
  if (!prepared) {
    Cost.GroundBuildPrepare.Took(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - prepareAt)
            .count());
    Error = std::move(prepared.error());
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  Cost.GroundBuildPrepare.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - prepareAt)
          .count());
  if (!*prepared) { return GroundBuildProgress::Pending; }
  state.MarkPrepared();
  return GroundBuildProgress::Pending;
}

namespace {
const Ground::OsmField *HeightCoverageVectors(GroundQuality quality,
                                              const Ground::OsmField *vectors) noexcept {
  return quality == GroundQuality::Refined ? vectors : nullptr;
}
}

Engine::State::GroundBuildProgress
Engine::State::BeginGroundSheetRefinement(const TangentFrame &standing, Patchwork &patchwork) {
  GroundBuildState &state = *World.GroundBuild;
  GroundBuildProducts &build = state.Candidate().Products();
  build.Sheets.Framed(standing);
  Published.Places(
      "ground refinement: source sheets", static_cast<double>(patchwork.Sheets.size()), "sheets");
  const Render::Viewpoint &eye = Picture.Standing->Watching();
  Generators::TerrainRefinementDetail detail{.EyeM = eye.EyeM};
  if (eye.Kind == Render::CameraKind::Orthographic) {
    detail.OrthographicPxPerM = static_cast<double>(Picture.Frame.HeightPx) / (2.0 * eye.YMagM);
  } else {
    detail.FocalPx =
        static_cast<double>(Picture.Frame.HeightPx) / (2.0 * std::tan(eye.YfovRad * 0.5));
  }
  std::vector<Generators::TerrainRefinementCorridor> roadCorridors;
  if (const auto *source = state.TransportSnapshot();
      source != nullptr && !state.RoadRouteIndices().empty()) {
    auto corridorCoverage =
        RoadRefinementCoverage::Build(*source, state.RoadRouteIndices(), standing);
    if (!corridorCoverage) {
      Error = std::move(corridorCoverage.error());
      World.GroundBuild.reset();
      return GroundBuildProgress::Failed;
    }
    roadCorridors = std::move(*corridorCoverage);
  }
  Published.Places(
      "ground refinement: road corridors", static_cast<double>(roadCorridors.size()), "edges");
  state.BeginsRefinement(std::make_unique<Generators::TerrainRefinementJob>(
      build.Sheets.BeginRefinement(patchwork,
                                   {.Side = Render::GroundLattice::kSide, .Halo = 1},
                                   detail,
                                   Render::GroundLattice::kPages,
                                   roadCorridors)));
  return GroundBuildProgress::Pending;
}

Engine::State::GroundBuildProgress Engine::State::AdvanceGroundSheets(const TangentFrame &standing,
                                                                      Patchwork &patchwork,
                                                                      const Around &coverage) {
  GroundBuildState &state = *World.GroundBuild;
  GroundBuildProducts &build = state.Candidate().Products();
  switch (state.CurrentSheetPhase()) {
    case Core::GroundBuildSchedule::SheetPhase::NeedsFields: {
      const auto prepared = build.Sheets.PrepareFields(
          patchwork,
          World.Stack.Ground(),
          {.FinestZoom = coverage.Zoom,
           .RequestsMost = kTerrainSheetsPerFrame,
           .Vectors = HeightCoverageVectors(state.Revision().Quality, World.Stack.Vectors()),
           .AdditionalTiles = state.RoadHeightTiles()});
      if (!prepared) {
        Error = prepared.error();
        World.GroundBuild.reset();
        return GroundBuildProgress::Failed;
      }
      if (*prepared) { state.AdvanceSheetPhase(); }
      return GroundBuildProgress::Pending;
    }
    case Core::GroundBuildSchedule::SheetPhase::NeedsRefinement: {
      if (state.RefinementJob() == nullptr) {
        return BeginGroundSheetRefinement(standing, patchwork);
      }
      auto refined = state.RefinementJob()->Advance(kTerrainRefinementSourcesPerFrame);
      if (!refined) {
        Error = std::move(refined.error());
        World.GroundBuild.reset();
        return GroundBuildProgress::Failed;
      }
      if (!*refined) { return GroundBuildProgress::Pending; }
      Published.Places("ground refinement: longest source selection slice",
                       state.RefinementJob()->LongestSelectionMs(),
                       "ms");
      Published.Places(
          "ground refinement: longest source", state.RefinementJob()->LongestSourceMs(), "ms");
      Published.Places(
          "ground refinement: patch deduplication", state.RefinementJob()->DeduplicationMs(), "ms");
      const auto replacementAt = std::chrono::steady_clock::now();
      patchwork.Sheets = std::move(*state.RefinementJob()).Take();
      Published.Places("ground refinement: patch replacement",
                       std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                 replacementAt)
                           .count(),
                       "ms");
      const auto releaseAt = std::chrono::steady_clock::now();
      state.FinishesRefinement();
      Published.Places(
          "ground refinement: job release",
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - releaseAt)
              .count(),
          "ms");
      Published.Places("ground: virtual tiles the lattice refines to",
                       static_cast<double>(std::ranges::count_if(
                           patchwork.Sheets, [](const Sheet &sheet) { return sheet.Virtual; })),
                       "tiles");
      state.AdvanceSheetPhase();
      return GroundBuildProgress::Pending;
    }
    case Core::GroundBuildSchedule::SheetPhase::NeedsHalos: {
      if (state.HaloJob() == nullptr) {
        state.BeginsHalos(
            std::make_unique<HeightSheets::HaloBuildJob>(build.Sheets, patchwork, coverage.Zoom));
      }
      const auto began = std::chrono::steady_clock::now();
      HeightSheets::HaloBuildJob &job = *state.HaloJob();
      const bool ready = job.Advance(kHaloNodesPerFrame);
      state.SamplesHaloSlice(
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count());
      if (!ready) { return GroundBuildProgress::Pending; }
      Published.Places(
          "ground: sheets the lattice haloed", static_cast<double>(job.Haloed()), "sheets");
      build.RimsMissing = build.Sheets.RimsMissing();
      Published.Places("ground: rims copied for want of a neighbour",
                       static_cast<double>(build.RimsMissing),
                       "sheets");
      Published.Places("ground candidate: longest halo slice", state.LongestHaloSliceMs(), "ms");
      state.FinishesHalos();
      state.AdvanceSheetPhase();
      return GroundBuildProgress::Pending;
    }
    case Core::GroundBuildSchedule::SheetPhase::NeedsMesh: {
      GroundBuildState::MeshBuild &meshing = state.InitialMeshing();
      const auto began = std::chrono::steady_clock::now();
      const size_t end =
          std::min(meshing.NextSheet + kTerrainSheetsPerFrame, patchwork.Sheets.size());
      for (; meshing.NextSheet < end; ++meshing.NextSheet) {
        Generators::AppendTerrainMeshSheet(meshing.Mesh,
                                           patchwork.Sheets[meshing.NextSheet],
                                           standing,
                                           {.Side = Render::GroundLattice::kSide, .Halo = 1});
      }
      const double sliceMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count();
      meshing.LongestSliceMs = std::max(meshing.LongestSliceMs, sliceMs);
      state.SamplesProductPeak();
      if (meshing.NextSheet < patchwork.Sheets.size()) { return GroundBuildProgress::Pending; }
      Published.Places(
          "ground candidate: longest initial mesh slice", meshing.LongestSliceMs, "ms");
      if (Session.Declared.Render.Audits) {
        const uint64_t sourceSheets = DigestPatchwork(patchwork);
        Published.Places("ground candidate: source sheets digest, low half",
                         static_cast<double>(sourceSheets & kLowWord),
                         "digest");
        Published.Places("ground candidate: source sheets digest, high half",
                         static_cast<double>(sourceSheets >> 32U),
                         "digest");
      }
      build.PositionsM = std::move(meshing.Mesh.PositionsM);
      build.Indices = std::move(meshing.Mesh.Indices);
      Core::ReportGroundRelief(Published,
                               {.TallestM = meshing.Mesh.TallestM,
                                .LowestM = meshing.Mesh.LowestM,
                                .TallestDistanceM = meshing.Mesh.TallestDistanceM});
      state.AdvanceSheetPhase();
      return GroundBuildProgress::Ready;
    }
    case Core::GroundBuildSchedule::SheetPhase::Ready: return GroundBuildProgress::Ready;
  }
  return GroundBuildProgress::Failed;
}

Engine::State::GroundBuildProgress Engine::State::AdvanceGroundClasses() {
  GroundBuildState &state = *World.GroundBuild;
  if (state.CurrentStage() != Core::GroundBuildSchedule::Stage::NeedsClasses) {
    return GroundBuildProgress::Ready;
  }
  GroundBuildProducts &build = state.Candidate().Products();
  static const Heap::Tag kClassingTag("ground-classify");
  const Heap::Tagged classing(kClassingTag);
  Classed classed = Classify(build.PositionsM, state.Candidate());
  build.ClassPalette = std::move(classed.Palette);
  build.ClassStructure = std::move(classed.Structure);
  state.AdvanceStage();
  return GroundBuildProgress::Pending;
}

Engine::State::GroundBuildProgress Engine::State::AdvanceGroundSurface() {
  GroundBuildState &state = *World.GroundBuild;
  if (state.CurrentStage() != Core::GroundBuildSchedule::Stage::NeedsGroundSurface) {
    return GroundBuildProgress::Ready;
  }
  GroundBuildProducts &build = state.Candidate().Products();
  Material bare;
  const Medium held = kEarthAir;
  for (int channel = 0; channel < 3; ++channel) {
    bare.BaseColour[channel] = held.GroundAlbedo[channel];
  }
  const auto ringSurface = build.Ground.addSurface("ground", bare);
  if (!ringSurface) {
    Error = Says::MaterialCreationFailed;
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  build.GroundMaterial = bare;
  build.GroundSurface = *ringSurface;
  state.AdvanceStage();
  return GroundBuildProgress::Pending;
}

Engine::State::GroundBuildProgress
Engine::State::AdvanceGroundBuildingModels(const TangentFrame &standing) {
  GroundBuildState &state = *World.GroundBuild;
  if (state.CurrentStage() != Core::GroundBuildSchedule::Stage::NeedsModels) {
    return GroundBuildProgress::Ready;
  }
  const auto began = std::chrono::steady_clock::now();
  Phasing clocks{.PhaseAt = began, .CensusAt = began, .WiresAt = began};
  static const Heap::Tag kModellingTag("ground-model");
  const Heap::Tagged modelling(kModellingTag);
  GroundWorldCandidate &candidate = state.Candidate();
  GroundBuildProducts &build = candidate.Products();
  if (!PrepareBuildingSurfaces(standing, build, clocks)) {
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  state.AdvanceStage();
  return GroundBuildProgress::Pending;
}

Engine::State::GroundBuildProgress Engine::State::AdvanceGroundStreetGraph() {
  GroundBuildState &state = *World.GroundBuild;
  if (state.CurrentStage() != Core::GroundBuildSchedule::Stage::NeedsNetwork) {
    return GroundBuildProgress::Ready;
  }
  GroundBuildProducts &build = state.Candidate().Products();
  const std::optional<GroundRevision> &publishedRevision = World.GroundPublished.Current();
  const GroundRevision &requestedRevision = state.Revision();
  const bool sourcesChanged =
      !publishedRevision || publishedRevision->Region != requestedRevision.Region ||
      publishedRevision->ResidentTiles != requestedRevision.ResidentTiles ||
      publishedRevision->VectorGeneration != requestedRevision.VectorGeneration ||
      publishedRevision->StreetTiles != requestedRevision.StreetTiles;
  if (build.StreetGraph != nullptr &&
      World.Stack.Ways().Ways().size() == build.StreetGraphWayCount && !sourcesChanged) {
    state.AdvanceStage();
    return GroundBuildProgress::Pending;
  }
  if (World.Stack.Vectors() == nullptr) {
    build.StreetGraph.reset();
    build.StreetGraphWayCount = 0;
    state.AdvanceStage();
    return GroundBuildProgress::Pending;
  }
  if (state.StreetGraphWorker() == nullptr) {
    const int sourceZoom = state.Coverage().Zoom;
    auto fields =
        std::make_shared<const SourcedTerrainFields>(build.Sheets.SnapshotSourcedFields());
    auto started = outshine::Ground::VectorStreetGraphBuildJob::Begin(
        World.Stack, [fields = std::move(fields), sourceZoom](LongitudeLatitude at) {
          return fields->AslMAt(sourceZoom, at);
        });
    if (!started) {
      Error = std::move(started.error());
      World.GroundBuild.reset();
      return GroundBuildProgress::Failed;
    }
    state.BeginStreetGraph(std::make_unique<VectorStreetGraphWorker>(std::move(*started)));
    return GroundBuildProgress::Pending;
  }
  auto completed = state.StreetGraphWorker()->Collect();
  if (!completed) { return GroundBuildProgress::Pending; }
  if (!*completed) {
    Error = std::move(completed->error());
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  const double longestSliceMs = completed->value().LongestSliceMs;
  const outshine::Ground::VectorStreetGraph::Built &mapped = completed->value().Graph;
  build.StreetGraph = mapped.Graph;
  build.StreetGraphWayCount = World.Stack.Ways().Ways().size();
  Published.Places("network: ways it holds", static_cast<double>(mapped.Ways), "ways");
  Published.Places("network: laying ways", mapped.LayMs, "ms");
  Published.Places("network: weaving topology", mapped.WeaveMs, "ms");
  Published.Places("network: beginning weave", mapped.BeginWeaveMs, "ms");
  Published.Places("network: cleaning weave temporaries", mapped.CleanupWeaveMs, "ms");
  Published.Places("network: longest weave cleanup slice", mapped.CleanupWeaveLongestMs, "ms");
  Published.Places("network: longest weave slice", mapped.WeaveLongestMs, "ms");
  Published.Places("network: longest way sort slice", mapped.WeaveSlices.SortMs, "ms");
  Published.Places("network: longest way merge slice", mapped.WeaveSlices.MergeMs, "ms");
  Published.Places("network: longest way reserve slice", mapped.WeaveSlices.ReserveMs, "ms");
  Published.Places("network: longest way copy slice", mapped.WeaveSlices.CopyMs, "ms");
  Published.Places("network: beginning snap", mapped.WeaveSlices.BeginSnapMs, "ms");
  Published.Places("network: longest snap slice", mapped.WeaveSlices.SnapMs, "ms");
  Published.Places("network: longest edge creation slice", mapped.WeaveSlices.EdgesMs, "ms");
  Published.Places("network: longest edge index slice", mapped.WeaveSlices.IndexMs, "ms");
  Published.Places("network: beginning adjacency", mapped.WeaveSlices.AdjacencyBeginMs, "ms");
  Published.Places("network: longest adjacency slice", mapped.WeaveSlices.AdjacencyMs, "ms");
  Published.Places("network: longest tie slice", mapped.WeaveSlices.TieMs, "ms");
  Published.Places("network: longest weave publish slice", mapped.WeaveSlices.PublishMs, "ms");
  Published.Places("network: classifying crossings", mapped.CrossingsMs, "ms");
  Published.Places("network: longest crossing slice", mapped.CrossingsLongestMs, "ms");
  Published.Places("network: longest crossing setup slice", mapped.CrossingSlices.SetupMs, "ms");
  Published.Places("network: longest crossing pair slice", mapped.CrossingSlices.TestMs, "ms");
  Published.Places("network: crossing publication", mapped.CrossingSlices.PublishMs, "ms");
  Published.Places("network: crossing point span", mapped.CrossingSweep.SpanMs, "ms");
  Published.Places("network: crossing segment list", mapped.CrossingSweep.SegmentsMs, "ms");
  Published.Places("network: crossing grid", mapped.CrossingSweep.GridMs, "ms");
  Published.Places("network: crossing cell filing", mapped.CrossingSweep.FilingMs, "ms");
  Published.Places("network: crossing pair tests", mapped.CrossingSweep.TestMs, "ms");
  Published.Places("network: crossing candidate pairs",
                   static_cast<double>(mapped.CrossingSweep.CandidatePairs),
                   "pairs");
  Published.Places("network: crossing cache", mapped.CrossingSweep.CacheMs, "ms");
  Published.Places("network: elevating nodes", mapped.ElevateMs, "ms");
  Published.Places("network: beginning elevation", mapped.BeginElevationMs, "ms");
  Published.Places("network: longest elevation slice", mapped.ElevateLongestMs, "ms");
  Published.Places(
      "network: longest node sample slice", mapped.ElevationSlices.SampleNodesMs, "ms");
  Published.Places(
      "network: longest point write slice", mapped.ElevationSlices.WritePointsMs, "ms");
  Published.Places("network: longest station slice", mapped.ElevationSlices.StationsMs, "ms");
  Published.Places("network: longest slope slice", mapped.ElevationSlices.SlopesMs, "ms");
  Published.Places("network: longest grade slice", mapped.ElevationSlices.GradesMs, "ms");
  Published.Places("network: publishing graph", mapped.PublishMs, "ms");
  Published.Places("network: longest build slice", longestSliceMs, "ms");
  Published.Places("network: nodes", static_cast<double>(mapped.Nodes), "nodes");
  Published.Places("network: edges", static_cast<double>(mapped.Edges), "edges");
  Published.Places("network: nodes where three or more edges meet",
                   static_cast<double>(mapped.Junctions),
                   "nodes");
  Published.Places(
      "network: points with a height", static_cast<double>(mapped.Elevated.Points), "points");
  Published.Places("network: points the ground refused a height",
                   static_cast<double>(mapped.Elevated.Refused),
                   "points");
  Published.Places("network: steepest grade", mapped.Elevated.SteepestGrade, "m/m");
  Published.Places(
      "network: steepest grade on a sealed way", mapped.Elevated.SteepestSealedGrade, "m/m");
  state.FinishStreetGraph();
  state.AdvanceStage();
  return GroundBuildProgress::Pending;
}

Engine::State::GroundBuildProgress
Engine::State::AdvanceGroundRoadAlignments(const TangentFrame &standing) {
  GroundBuildState &state = *World.GroundBuild;
  if (state.CurrentStage() != Core::GroundBuildSchedule::Stage::NeedsRoadAlignments) {
    return GroundBuildProgress::Ready;
  }
  if (state.RoadRouteIndices().empty()) {
    state.AdvanceStage();
    return GroundBuildProgress::Pending;
  }
  if (!World.Pool || state.TransportSnapshot() == nullptr) {
    Error = "road alignment candidate has no task pool or OSM source";
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  GroundBuildProducts &build = state.Candidate().Products();
  if (state.RoadSurfaceBuilt()) {
    if (state.NextRoadSurfaceTransfer() < build.RoadAlignments.size()) {
      const NamedRoadAlignment &route = build.RoadAlignments[state.NextRoadSurfaceTransfer()];
      if (!route.Surface || !build.Ground.append(route.Surface->SurfaceGeometry)) {
        Error =
            std::format("road surface '{}' could not enter the native ground geometry", route.Id);
        World.GroundBuild.reset();
        return GroundBuildProgress::Failed;
      }
      state.AdvanceRoadSurfaceTransfer();
      state.SamplesProductPeak();
      return GroundBuildProgress::Pending;
    }
    state.AdvanceStage();
    return GroundBuildProgress::Pending;
  }
  if (!state.RoadAlignmentRequested()) {
    RoadAlignmentBuildRequest request{
        .Source = state.TransportSnapshotOwner(),
        .Terrain = state.Candidate().Products().Sheets.SnapshotSourcedFields(),
        .RouteIndices = {state.RoadRouteIndices().begin(), state.RoadRouteIndices().end()},
        .RenderFrame = standing,
        .TerrainZoom = state.Coverage().Zoom,
        .CandidateGeneration = state.Id()};
    if (!World.RoadAlignmentBuilds.TryStart(*World.Pool, std::move(request))) {
      return GroundBuildProgress::Pending;
    }
    state.MarkRoadAlignmentRequested();
    return GroundBuildProgress::Pending;
  }
  auto result = World.RoadAlignmentBuilds.TakeCompleted();
  if (!result) { return GroundBuildProgress::Pending; }
  if (!*result) {
    const RoadAlignmentBuildError &failure = result->error();
    Error = std::format("road alignment '{}' failed with code {} at edge {}",
                        failure.RouteId,
                        static_cast<int>(failure.Code),
                        failure.Edge.WayId);
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  if (!result->value().Matches(state.Id(), state.TransportSnapshot()->SourceIdentity())) {
    Error = "road alignment candidate completed for a different source revision";
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  build.RoadAlignments = std::move(result->value().Routes);
  state.HoldRoadEarthworks(std::move(result->value().Earthworks));
  size_t alignedEdges = 0;
  size_t terrainSources = 0;
  for (const NamedRoadAlignment &route : build.RoadAlignments) {
    alignedEdges += route.Alignment->Edges().size();
    terrainSources += route.Alignment->TerrainSources().size();
  }
  Published.Places(
      "semantic road alignments built", static_cast<double>(build.RoadAlignments.size()), "routes");
  Published.Places(
      "semantic road alignment source edges", static_cast<double>(alignedEdges), "edges");
  Published.Places(
      "semantic road alignment terrain sources", static_cast<double>(terrainSources), "sources");
  state.MarkRoadSurfaceBuilt();
  return GroundBuildProgress::Pending;
}

Engine::State::GroundBuildProgress Engine::State::AdvanceGroundStructureBakes() {
  GroundBuildState &state = *World.GroundBuild;
  if (state.CurrentStage() != Core::GroundBuildSchedule::Stage::NeedsBakes) {
    return GroundBuildProgress::Ready;
  }
  const bool ready = state.Revision().Quality == GroundQuality::Refined
                         ? World.StructureBuilds.SourcesComplete(World.Stack, state.Footprints())
                         : StructuresReady(state.Footprints(), state.Revision());
  if (!ready) { return GroundBuildProgress::Pending; }
  GroundBuildProducts &build = state.Candidate().Products();
  const auto fieldsAt = std::chrono::steady_clock::now();
  build.Sheets.ForgetsFields();
  Published.Places(
      "ground candidate: source field release",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - fieldsAt)
          .count(),
      "ms");
  state.AdvanceStage();
  return GroundBuildProgress::Pending;
}

std::string_view Engine::State::GroundBuildStatus() const noexcept {
  return World.GroundBuild ? World.GroundBuild->Status() : "absent";
}

std::string Engine::State::GroundBuildDiagnostic() const {
  std::string diagnostic = "ground build=" + std::string(GroundBuildStatus());
  if (!World.GroundBuild) { return diagnostic; }
  const auto &footprints = World.GroundBuild->Footprints();
  const auto *vectors = World.Stack.Vectors();
  diagnostic += ", structure refinement=" + std::to_string(footprints.RefinementRemaining());
  if (const auto *worker = World.GroundBuild->StreetGraphWorker(); worker != nullptr) {
    diagnostic += ", network phase=" + std::string(worker->PhaseName()) +
                  ", network advances=" + std::to_string(worker->Advances());
  }
  diagnostic +=
      ", footprints ingested=" +
      std::to_string(static_cast<int>(vectors != nullptr && footprints.Ingested(*vectors)));
  return diagnostic;
}

size_t Engine::State::StructureCandidatesMost() const noexcept {
  return (!World.GroundPublished.Current() && !World.GroundBuild) ||
                 (World.GroundBuild &&
                  World.GroundBuild->Revision().Quality == GroundQuality::Playable)
             ? kPlayableStructureCandidates
             : kRefinedStructureCandidates;
}

Ground::BuildingField *Engine::State::CandidateFootprints() const noexcept {
  return World.GroundBuild ? &World.GroundBuild->Footprints() : nullptr;
}

bool Engine::State::StagesGroundBakes(size_t landsMost) {
  if (!World.GroundBuild ||
      (World.GroundBuild->CurrentStage() != Core::GroundBuildSchedule::Stage::NeedsCorridors &&
       World.GroundBuild->CurrentStage() != Core::GroundBuildSchedule::Stage::NeedsBakes)) {
    return true;
  }
  GroundBuildState &state = *World.GroundBuild;
  GroundWorldCandidate &candidate = state.Candidate();
  GroundBuildProducts &build = candidate.Products();
  Published.Places("ground candidate: structure tiles to certify",
                   static_cast<double>(state.Footprints().RefinementRemaining()),
                   "tiles");
  Published.Places(
      "ground candidate: structure field ingested",
      World.Stack.Vectors() != nullptr && state.Footprints().Ingested(*World.Stack.Vectors()) ? 1.0
                                                                                              : 0.0,
      "bool");
  const auto heights = state.Revision().Quality == GroundQuality::Refined
                           ? StructureBuildQueue::HeightRequirement::FineOnly
                           : StructureBuildQueue::HeightRequirement::AllowFallback;
  const int finestZoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  const StructureBuildQueue::HeightSource heightAt{
      .Sample = [&build,
                 finestZoom](LongitudeLatitude at) { return build.Sheets.AslMAt(finestZoom, at); },
      .CopyField =
          [&build](Data::TileId tile, Ground::HeightField::Block &into) {
            return build.Sheets.CopySourcedField(tile, into);
          },
      .Revision = {.Value = state.Id()}};
  const auto landingAt = std::chrono::steady_clock::now();
  auto ready =
      World.StructureBuilds.NextLandings(World.Stack,
                                         state.Footprints(),
                                         CurrentGeographicFocus(),
                                         heightAt.Revision,
                                         landsMost,
                                         heights,
                                         std::nullopt,
                                         StructureBuildQueue::BuildPurpose::SourceGeometry);
  Cost.BakeLanding.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - landingAt)
          .count());
  if (!ready) {
    Error = Generators::Describe(ready.error());
    return false;
  }
  const auto transferAt = std::chrono::steady_clock::now();
  if (!build.Surfaces) {
    Error = "structure materials were not registered before baking";
    return false;
  }
  build.Pieces.Wears(*build.Surfaces);
  for (const StructureBuildQueue::Landing &landing : *ready) {
    if (!build.Pieces.Hands(
            landing.Tile, *landing.Baked, landing.AnchorEcef, Error, landing.SourceKey)) {
      return false;
    }
  }
  const double transferMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - transferAt)
          .count();
  Cost.BakeTransfer.Took(transferMs);
  Cost.BakeCandidateTransfer.Took(transferMs);
  const auto commitAt = std::chrono::steady_clock::now();
  World.StructureBuilds.CommitsLandings(World.Stack, state.Footprints(), *ready);
  Cost.BakeCommit.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - commitAt)
          .count());
  const auto postingAt = std::chrono::steady_clock::now();
  (void)World.StructureBuilds.Posts(World.Stack,
                                    state.Footprints(),
                                    CurrentGeographicFocus(),
                                    heightAt,
                                    StructureCandidatesMost(),
                                    heights,
                                    std::nullopt,
                                    StructureBuildQueue::BuildPurpose::SourceGeometry);
  Cost.BakePosting.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - postingAt)
          .count());
  return true;
}

Engine::State::GroundBuildProgress Engine::State::AdvanceGroundPatchwork(const Around &coverage) {
  GroundBuildState &state = *World.GroundBuild;
  if (state.Laid() != nullptr) { return GroundBuildProgress::Ready; }
  static const Heap::Tag kPatchingTag("ground-patchwork");
  const Heap::Tagged patching(kPatchingTag);
  auto made = World.Shipping.Covering().Lay(World.Stack.Pool(), coverage);
  if (!made) {
    Error = made.error();
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  if (made->ContactPending != 0 || made->Sheets.empty() ||
      (state.Revision().Quality == GroundQuality::Refined && made->Pending != 0)) {
    if (made->Pending != 0) { return GroundBuildProgress::Pending; }
    Error = Says::GroundContactIncomplete;
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  state.Lays(*std::move(made));
  return GroundBuildProgress::Pending;
}

bool Engine::State::BuildGroundCorridors(const TangentFrame &standing,
                                         const Around &coverage,
                                         GroundBuildState &state) {
  const auto began = std::chrono::steady_clock::now();
  if (state.RetiringCorridors()) {
    const auto retireAt = std::chrono::steady_clock::now();
    const bool retired = state.CorridorJob()->RetireStep(kCorridorRetireUnitsPerFrame);
    if (retired) { state.FinishesCorridors(); }
    state.SamplesCorridorRetirement(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - retireAt)
            .count());
    state.SamplesCorridorSlice(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
            .count());
    if (!retired) { return true; }
    state.AdvanceStage();
    Published.Places(
        "ground candidate: corridor job retirement", state.LongestCorridorRetirementMs(), "ms");
    Published.Places(
        "ground candidate: longest corridor slice", state.LongestCorridorSliceMs(), "ms");
    return true;
  }
  GroundBuildProducts &build = state.Candidate().Products();
  const TriangleBvh surface;
  Published.Places("ground: triangles the drape can reach",
                   static_cast<double>(build.Indices.size()) / 3.0,
                   "triangles");
  Drape drapedOver{.Surface = surface, .Field = {}};
  size_t fieldMisses = 0;
  std::optional<Drape::EastNorth> firstFieldMiss;
  drapedOver.Field = [&coverage, &build, &fieldMisses, &firstFieldMiss](Drape::EastNorth at) {
    const std::optional<double> sampled = build.Sheets.FieldUpM(coverage.Zoom, at);
    fieldMisses += sampled ? 0u : 1u;
    if (!sampled && !firstFieldMiss) { firstFieldMiss = at; }
    return sampled;
  };
  std::vector<EarthworkStamp> corridors;
  std::vector<DiagnosticSample> notes;
  const Generators::Corridors::Site site{.Vectors = World.Stack.Vectors(),
                                         .Ways = World.Stack.Ways(),
                                         .Materials = World.Stack.Materials(),
                                         .Vegetation = World.Stack.Vegetation(),
                                         .GroundClasses = &World.Stack.Classes(),
                                         .Ground = &World.Stack.Ground(),
                                         .Network = build.StreetGraph.get(),
                                         .Standing = standing,
                                         .Draped = drapedOver,
                                         .Classes = build.ClassStructure,
                                         .CensusAt = state.Began(),
                                         .EyeLatDeg = coverage.LatitudeDeg,
                                         .EyeLonDeg = coverage.LongitudeDeg,
                                         .FocalPx = build.Footprints.FocalPx()};
  if (state.CorridorJob() == nullptr) {
    const auto jobAt = std::chrono::steady_clock::now();
    state.BeginsCorridors(Generators::Corridors::Begin(site));
    Published.Places(
        "ground candidate: corridor job admission",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - jobAt).count(),
        "ms");
    const auto inventoryAt = std::chrono::steady_clock::now();
    state.SamplesProductPeak();
    Published.Places(
        "ground candidate: corridor product inventory",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inventoryAt)
            .count(),
        "ms");
    state.SamplesCorridorSlice(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
            .count());
    return true;
  }
  const auto paved = World.Shipping.Corridors().Advance(*state.CorridorJob(),
                                                        site,
                                                        kCorridorLanesPerFrame,
                                                        kCorridorNodesPerFrame,
                                                        build.Ground,
                                                        &corridors,
                                                        &notes);
  state.SamplesProductPeak();
  if (!paved) {
    Error = paved.error();
    return false;
  }
  if (fieldMisses > 0) {
    if (firstFieldMiss) {
      const Ground::OsmField *const shapes = World.Stack.Vectors();
      const int vectorZoom =
          shapes != nullptr && !shapes->Tiles().empty() ? shapes->Tiles().front().Z : -1;
      Error = std::format("{}: east {:.3f} m, north {:.3f} m, {} misses, sheet zoom {}, "
                          "DEM zoom {}, OSM zoom {}",
                          Says::CorridorTerrainIncomplete,
                          firstFieldMiss->EastM,
                          firstFieldMiss->NorthM,
                          fieldMisses,
                          coverage.Zoom,
                          World.Stack.Ground().BlockZoom(),
                          vectorZoom);
    } else {
      Error = Says::CorridorTerrainIncomplete;
    }
    return false;
  }
  if (!*paved) {
    state.SamplesCorridorSlice(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
            .count());
    return true;
  }
  for (const DiagnosticSample &one : notes) {
    Published.Places(one.Name, one.Value, one.Unit.c_str());
  }
  Published.Places(
      "ground candidate: corridor drape field misses", static_cast<double>(fieldMisses), "queries");
  state.HoldsCorridors(std::move(corridors));
  Published.Places("ground candidate: corridors", state.CorridorJob()->WorkMs(), "ms");
  state.BeginsCorridorRetirement();
  state.SamplesCorridorSlice(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  return true;
}

bool Engine::State::BuildGroundTerrainMesh(const TangentFrame &standing,
                                           Patchwork &patchwork,
                                           GroundBuildState &state) {
  const auto began = std::chrono::steady_clock::now();
  GroundBuildProducts &build = state.Candidate().Products();
  GroundBuildState::MeshBuild &meshing = state.Meshing();
  if (!meshing.SheetsStitched) {
    if (!build.Sheets.Stitch(patchwork, Error)) { return false; }
    meshing.SheetsStitched = true;
    Published.Places(
        "ground candidate: sheet stitching",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
        "ms");
    return true;
  }
  if (!meshing.ResidencyStarted) {
    if (!build.Sheets.BeginResidency(patchwork, Error)) { return false; }
    meshing.ResidencyStarted = true;
    Published.Places(
        "ground candidate: residency preparation",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
        "ms");
    return true;
  }
  if (!meshing.ResidencyReady) {
    const auto advanced = build.Sheets.AdvanceResidency(patchwork, kTerrainResidencySheetsPerFrame);
    if (!advanced) {
      Error = advanced.error();
      return false;
    }
    const double sliceMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    meshing.LongestResidencySliceMs = std::max(meshing.LongestResidencySliceMs, sliceMs);
    state.SamplesProductPeak();
    if (!*advanced) { return true; }
    meshing.ResidencyReady = true;
    Published.Places(
        "ground candidate: longest residency slice", meshing.LongestResidencySliceMs, "ms");
    Published.Places(
        "ground candidate: longest residency batch", build.Sheets.LongestResidencyBatchMs(), "ms");
    Published.Places(
        "ground candidate: residency finalize", build.Sheets.ResidencyFinalizeMs(), "ms");
    return true;
  }
  const size_t end = std::min(meshing.NextSheet + kTerrainSheetsPerFrame, patchwork.Sheets.size());
  for (; meshing.NextSheet < end; ++meshing.NextSheet) {
    Generators::AppendTerrainMeshSheet(meshing.Mesh,
                                       patchwork.Sheets[meshing.NextSheet],
                                       standing,
                                       {.Side = Render::GroundLattice::kSide, .Halo = 1});
  }
  const double sliceMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  meshing.LongestSliceMs = std::max(meshing.LongestSliceMs, sliceMs);
  state.SamplesProductPeak();
  if (meshing.NextSheet < patchwork.Sheets.size()) { return true; }
  Published.Places("ground candidate: longest native mesh slice", meshing.LongestSliceMs, "ms");
  build.PositionsM = std::move(meshing.Mesh.PositionsM);
  build.Indices = std::move(meshing.Mesh.Indices);
  Published.Places(
      "ground: height pages standing", static_cast<double>(build.Sheets.Standing()), "pages");
  Published.Places(
      "ground: tiles the lattice draws", static_cast<double>(build.Sheets.Instances()), "tiles");
  for (const auto &[name, kind] : {std::pair{"virtual", build.Sheets.Seams().Virtual},
                                   std::pair{"real", build.Sheets.Seams().Real}}) {
    const std::string at = std::string("ground: seam, ") + name + ", ";
    Published.Places(at + "edges stitched", static_cast<double>(kind.Edges), "edges");
    Published.Places(at + "even nodes off the coarser node, worst", kind.EvenM, "m");
    Published.Places(
        at + "odd nodes off the coarser chord before the stitch, worst", kind.OddBeforeM, "m");
    Published.Places(at + "odd nodes off the coarser chord after it, worst", kind.OddAfterM, "m");
    if (kind.OddBeforeM > 0.0) {
      Published.Places(at + "worst odd node longitude", kind.WorstLongitudeDeg, "deg");
      Published.Places(at + "worst odd node latitude", kind.WorstLatitudeDeg, "deg");
      Published.Places(at + "worst odd node fine zoom", kind.WorstFineZoom, "zoom");
      Published.Places(at + "worst odd node coarse zoom", kind.WorstCoarseZoom, "zoom");
    }
  }
  const uint64_t sheets = build.Sheets.Digest();
  Published.Places(
      "ground: the sheets' digest, low half", static_cast<double>(sheets & kLowWord), "digest");
  Published.Places(
      "ground: the sheets' digest, high half", static_cast<double>(sheets >> 32U), "digest");
  Published.Places("ground: sheets NOT drawn for want of nodes",
                   static_cast<double>(build.Sheets.Flat()),
                   "tiles");
  state.AdvanceStage();
  Published.Places(
      "ground candidate: terrain mesh",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
      "ms");
  return true;
}

bool Engine::State::PublishGroundGeometry(GroundBuildState &state) {
  const auto sliceBegan = std::chrono::steady_clock::now();
  GroundWorldCandidate &candidate = state.Candidate();
  GroundBuildProducts &build = candidate.Products();
  const auto sample = [&] {
    state.SamplesGeometrySlice(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sliceBegan)
            .count());
  };
  if (state.GeometrySubmissionStep() == GroundBuildState::GeometrySubmission::Classes) {
    const auto classesBegan = std::chrono::steady_clock::now();
    Render::GroundClassUploadMetrics upload;
    const bool hasClasses = build.ClassStructure && !build.ClassPalette.empty();
    if (hasClasses && !candidate.BeginGroundClasses(
                          build.ClassStructure, std::move(build.ClassPalette), Error, &upload)) {
      return false;
    }
    state.ClassesStarted();
    state.SamplesClassComponents(upload);
    state.SamplesClassUploadSlice(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - classesBegan)
            .count());
    if (!hasClasses) {
      state.ClassesUploaded();
      Published.Places("ground candidate: class upload", state.TotalClassUploadMs(), "ms");
    }
    sample();
    return true;
  }
  if (state.GeometrySubmissionStep() == GroundBuildState::GeometrySubmission::ClassRanges) {
    Render::GroundClassUploadMetrics upload;
    auto advanced = candidate.AdvanceGroundClasses(kClassUploadBytesPerFrame, &upload);
    if (!advanced) {
      Error = std::move(advanced.error());
      return false;
    }
    state.SamplesClassComponents(upload);
    state.SamplesClassUploadSlice(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sliceBegan)
            .count());
    if (*advanced) {
      const Render::GroundClassUploadMetrics &longest = state.LongestClassComponents();
      Published.Places("ground class upload: allocation", longest.Storage.AllocationMs, "ms");
      Published.Places("ground class upload: largest range",
                       static_cast<double>(longest.Storage.BytesSubmitted),
                       "bytes");
      Published.Places("ground class upload: staging", longest.Storage.StagingMs, "ms");
      Published.Places("ground class upload: submission", longest.Storage.SubmissionMs, "ms");
      Published.Places(
          "ground class upload: source preparation", longest.SourcePreparationMs, "ms");
      Published.Places("ground class upload: restore source", longest.RestoreSourceMs, "ms");
      Published.Places("ground candidate: class upload", state.TotalClassUploadMs(), "ms");
      Published.Places(
          "ground candidate: longest class upload slice", state.LongestClassUploadMs(), "ms");
      state.ClassesUploaded();
    }
    sample();
    return true;
  }
  if (state.GeometrySubmissionStep() == GroundBuildState::GeometrySubmission::Begin) {
    const size_t drivenParts = Picture.Standing->DrivenParts();
    Published.Places("restand: the carried count the world hands over",
                     static_cast<double>(drivenParts),
                     "carried");
    Published.Places(
        "restand: parts in the geometry", static_cast<double>(build.Ground.parts()), "parts");
    candidate.Digests(Session.Declared.Render.Audits);
    size_t handed = 0;
    for (int part = 0; part < build.Ground.parts(); ++part) {
      handed += build.Ground.trianglesOf(part).size() / 3u;
    }
    Published.Places(
        "the triangles handed to the renderer", static_cast<double>(handed), "triangles");
    Published.Places("in this many parts", static_cast<double>(build.Ground.parts()), "parts");
    auto began = candidate.BeginGroundGeometryBuild(
        std::move(build.Ground), build.GroundSurface, build.GroundMaterial);
    if (!began) {
      Error = std::move(began.error());
      return false;
    }
    state.GeometryStarted();
    sample();
    return true;
  }
  assert(candidate.GroundGeometryBuildActive());
  auto advanced = candidate.AdvanceGroundGeometryBuild(kShapeCookItemsPerFrame);
  const double sliceMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sliceBegan)
          .count();
  state.SamplesGeometrySlice(sliceMs);
  if (!advanced) {
    Error = std::move(advanced.error());
    return false;
  }
  if (!*advanced) { return true; }
  Published.Places("ground candidate: final scene geometry slice", sliceMs, "ms");
  Published.Places(
      "ground candidate: longest scene geometry slice", state.LongestGeometrySliceMs(), "ms");
  state.AdvanceStage();
  return true;
}

bool Engine::State::AdvancesGroundRetirement() {
  if (!World.GroundRetirement) { return true; }
  const auto retirementAt = std::chrono::steady_clock::now();
  const bool retired = World.GroundRetirement->AdvancesRetirement(kGroundRestorePagesPerFrame);
  if (retired) { World.GroundRetirement.reset(); }
  Cost.GroundRetirement.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - retirementAt)
          .count());
  return retired;
}

bool Engine::State::GroundInputsReady(GroundQuality quality) const {
  return Session.Declared.Ground.Declared && Picture.Standing != nullptr && World.Stack.Opened() &&
         GroundSourcesReady(World.Stack, quality);
}

bool Engine::State::AdvancesGroundWithinBudget(GroundQuality quality) {
  constexpr size_t kAdvancesMost = 4;
  constexpr double kGroundBudgetMs = 8.0;
  const auto began = std::chrono::steady_clock::now();
  for (size_t advance = 0; advance < kAdvancesMost; ++advance) {
    const GroundBuildState *const before = World.GroundBuild.get();
    const uint64_t id = before != nullptr ? before->Id() : 0;
    const size_t phase = before != nullptr ? before->Progress() : 0;
    const auto submission = before != nullptr ? before->GeometrySubmissionStep()
                                              : GroundBuildState::GeometrySubmission::Classes;
    const uint64_t slices = before != nullptr && phase < Cost.GroundPhases.size()
                                ? Cost.GroundPhases[phase].Taken()
                                : 0;
    if (!Grounds(false, quality)) { return false; }
    const GroundBuildState *const after = World.GroundBuild.get();
    if (after == nullptr || after->Id() != id || after->Progress() != phase ||
        after->GeometrySubmissionStep() != submission ||
        submission == GroundBuildState::GeometrySubmission::ClassRanges ||
        Cost.GroundPhases[phase].Taken() == slices) {
      return true;
    }
    if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
            .count() >= kGroundBudgetMs) {
      return true;
    }
  }
  return true;
}

Engine::State::GroundBuildProgress Engine::State::AdvanceGroundConstructionStages(
    const TangentFrame &standing, Patchwork &patchwork, GroundBuildState &state) {
  switch (state.CurrentStage()) {
    case Core::GroundBuildSchedule::Stage::NeedsCorridors:
      return BuildGroundCorridors(standing, state.Coverage(), state) ? GroundBuildProgress::Pending
                                                                     : GroundBuildProgress::Failed;
    case Core::GroundBuildSchedule::Stage::NeedsEarthworks:
      return PressGroundEarthworks(standing, patchwork, state) ? GroundBuildProgress::Pending
                                                               : GroundBuildProgress::Failed;
    case Core::GroundBuildSchedule::Stage::NeedsTerrainMesh:
      return BuildGroundTerrainMesh(standing, patchwork, state) ? GroundBuildProgress::Pending
                                                                : GroundBuildProgress::Failed;
    case Core::GroundBuildSchedule::Stage::NeedsWater: {
      const auto began = std::chrono::steady_clock::now();
      GroundBuildProducts &build = state.Candidate().Products();
      if (!BuildWaterSurfaces(standing, build.Ground, build.GroundSurface)) {
        return GroundBuildProgress::Failed;
      }
      state.AdvanceStage();
      Published.Places(
          "ground candidate: water",
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count(),
          "ms");
      return GroundBuildProgress::Pending;
    }
    case Core::GroundBuildSchedule::Stage::NeedsGeometry:
      return PublishGroundGeometry(state) ? GroundBuildProgress::Pending
                                          : GroundBuildProgress::Failed;
    default: return GroundBuildProgress::Ready;
  }
}

bool Engine::State::Grounds(bool alsoWhenTilesLanded, GroundQuality quality) {
  static const Heap::Tag kLayingTag("world-ground");
  const Heap::Tagged laying(kLayingTag);
  if (World.Pool) {
    World.RoadAlignmentBuilds.Poll(*World.Pool, World.GroundBuild ? World.GroundBuild->Id() : 0);
  }
  if (!AdvancesGroundRetirement()) { return true; }
  if (!GroundInputsReady(quality)) { return true; }
  const Scenario::Document &declared = Session.Declared;
  const double anchorLat = declared.Ground.Origin.LatitudeDeg;
  const double anchorLon = declared.Ground.Origin.LongitudeDeg;

  const auto requestAt = std::chrono::steady_clock::now();
  const auto asked = RingWanted(alsoWhenTilesLanded, quality);
  Cost.GroundRequest.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - requestAt)
          .count());
  if (!asked) { return asked.error() == Laid::Unchanged || asked.error() == Laid::Pending; }
  const auto buildAt = std::chrono::steady_clock::now();
  const GroundBuildProgress progress = AdvanceGroundCandidatePreparation(*asked);
  Cost.GroundBuildBegin.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - buildAt)
          .count());
  if (progress != GroundBuildProgress::Ready) { return progress != GroundBuildProgress::Failed; }
  GroundBuildState &state = *World.GroundBuild;
  const size_t phase = state.Progress();
  assert(phase < Cost.GroundPhases.size());
  const ScopedCounter phaseTime(Cost.GroundPhases[phase]);
  const Around &over = state.Coverage();
  GroundWorldCandidate &candidate = state.Candidate();
  const auto rebuildBegan = state.Began();

  const GroundBuildProgress patchwork = AdvanceGroundPatchwork(over);
  if (patchwork != GroundBuildProgress::Ready) { return patchwork != GroundBuildProgress::Failed; }
  Patchwork &laid = *state.Laid();

  const double frameLat = anchorLat;
  const double frameLon = anchorLon;
  const TangentFrame standing =
      TangentFrame::At({.LongitudeDeg = frameLon, .LatitudeDeg = frameLat});
  const GroundBuildProgress sheetProgress = AdvanceGroundSheets(standing, laid, over);
  if (sheetProgress != GroundBuildProgress::Ready) {
    return sheetProgress != GroundBuildProgress::Failed;
  }
  const GroundBuildProgress classes = AdvanceGroundClasses();
  if (classes == GroundBuildProgress::Failed) { return false; }
  const GroundBuildProgress materialProgress = AdvanceGroundSurface();
  if (materialProgress != GroundBuildProgress::Ready) {
    return materialProgress != GroundBuildProgress::Failed;
  }
  const GroundBuildProgress models = AdvanceGroundBuildingModels(standing);
  if (models != GroundBuildProgress::Ready) { return models != GroundBuildProgress::Failed; }
  const GroundBuildProgress streetGraph = AdvanceGroundStreetGraph();
  if (streetGraph != GroundBuildProgress::Ready) {
    return streetGraph != GroundBuildProgress::Failed;
  }
  const GroundBuildProgress roads = AdvanceGroundRoadAlignments(standing);
  if (roads != GroundBuildProgress::Ready) { return roads != GroundBuildProgress::Failed; }
  const GroundBuildProgress bakes = AdvanceGroundStructureBakes();
  if (bakes != GroundBuildProgress::Ready) { return bakes != GroundBuildProgress::Failed; }
  const GroundBuildProgress construction = AdvanceGroundConstructionStages(standing, laid, state);
  if (construction != GroundBuildProgress::Ready) {
    return construction != GroundBuildProgress::Failed;
  }
  if (state.CurrentStage() != Core::GroundBuildSchedule::Stage::NeedsPublication) {
    Error = "ground candidate reached an invalid stage";
    return false;
  }
  auto phaseAt = std::chrono::steady_clock::now();
  state.PublishesFootprints();
  if (auto published =
          candidate.Publish(World, World.Stack.Footprints(), Picture.Standing, state.Revision());
      !published) {
    Error = std::move(published.error());
    World.GroundBuild.reset();
    return false;
  }
  Published.Places(
      "ground candidate: publication",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count(),
      "ms");
  const GroundWorldCandidate::PublicationMetrics &publication = candidate.Publication();
  Published.Places("ground publication: world owner swap", publication.WorldMs, "ms");
  Published.Places("ground publication: CPU products", publication.ProductsMs, "ms");
  Published.Places("ground publication: pieces", publication.PiecesMs, "ms");
  Published.Places("ground publication: orphan structure pieces reclaimed",
                   static_cast<double>(publication.OrphanStructurePieces),
                   "pieces");
  Published.Places("ground publication: resource binding", publication.BindingMs, "ms");
  Published.Places("ground publication: revision", publication.RevisionMs, "ms");
  Published.Places("ground publication: quality",
                   state.Revision().Quality == GroundQuality::Refined ? 1.0 : 0.0,
                   "0=playable 1=refined");
  const uint64_t geometry = World.Pieces.Digest();
  Published.Places(
      "the geometry the world built, high half", static_cast<double>(geometry >> 32U), "digest");
  Published.Places("and its low half", static_cast<double>(geometry & kLowWord), "digest");
  Published.Places("ground candidate: direct CPU product peak",
                   static_cast<double>(state.ProductPeakBytes()),
                   "bytes");
  Published.Places("ground candidate: CPU products retained for retirement",
                   static_cast<double>(state.RetainedProductBytes()),
                   "bytes");
  Published.Places("ground candidate: candidate products retained for retirement",
                   static_cast<double>(state.RetainedCandidateBytes()),
                   "bytes");
  Published.Places("ground candidate: patchwork retained for retirement",
                   static_cast<double>(state.RetainedPatchworkBytes()),
                   "bytes");
  Published.Places("ground candidate: mesh products retained for retirement",
                   static_cast<double>(state.RetainedMeshBytes()),
                   "bytes");
  assert(!World.GroundRetirement);
  World.GroundRetirement = std::move(World.GroundBuild);
  Published.Places(
      "rebuild: of that, walking it into the proxy", Picture.Standing->BuildMs(), "ms");
  const Core::GeometryBuildSliceMetrics &slices = Picture.Standing->GeometrySlices();
  Published.Places("geometry slice: shape cooking", slices.CookMs, "ms");
  Published.Places("geometry slice: scene planning", slices.PlanMs, "ms");
  Published.Places("geometry slice: subject binding", slices.BindMs, "ms");
  Published.Places("geometry slice: draw planning", slices.DrawPlanMs, "ms");
  Published.Places("geometry slice: CPU packing", slices.PackMs, "ms");
  Published.Places("geometry slice: index upload", slices.IndexMs, "ms");
  Published.Places("geometry slice: stream completion", slices.FinishMs, "ms");
  Published.Places("geometry slice: finalization", slices.FinalizeMs, "ms");
  Published.Places("rebuild: standing render plan", Picture.Standing->PlanMs(), "ms");
  Published.Places("rebuild: of THAT, copying the subject", Picture.Standing->CarryMs(), "ms");
  Published.Places(
      "rebuild: standing and submitting INSIDE Build", Picture.Standing->InsideMs(), "ms");
  Published.Places("rebuild: shaping what was built", Picture.Standing->ReshapeMs(), "ms");
  Published.Places("rebuild: composing it", Picture.Standing->ComposeMs(), "ms");
  Published.Places("stand: shaping it a second time", Picture.Standing->ReshapeAgainMs(), "ms");
  Published.Places("stand: the proxy taking it", Picture.Standing->ProxyStandsMs(), "ms");
  Published.Places("stand: placing every part", Picture.Standing->PlacesMs(), "ms");
  Published.Places("stand: dressing them", Picture.Standing->WearsMs(), "ms");
  Published.Places("stand: their emitted radiance", Picture.Standing->LampsMs(), "ms");
  Published.Places("stand: the lamps and the key", Picture.Standing->LitMs(), "ms");
  Published.Places("stand: the medium's own tables", Picture.Standing->MediumMs(), "ms");

  Published.Places("stand: times the sky was integrated",
                   static_cast<double>(Picture.Standing->SkyIntegrations()),
                   "integrations");
  Published.Places("stand: sweeping the bounds to frame it", Picture.Standing->FramingMs(), "ms");
  Published.Places("rebuild: resolving its surface", Picture.Standing->ResolveMs(), "ms");
  Published.Places("rebuild: and its bounds", Picture.Standing->BoundsMs(), "ms");
  Published.Places(
      "rebuild: cutting it into clusters", Picture.Standing->Clustering().BuildMs, "ms");
  Published.Places("cook: clusters with no parent above them",
                   static_cast<double>(Picture.Standing->Clustering().RootClusters),
                   "clusters");
  Published.Places("cook: clusters in all",
                   static_cast<double>(Picture.Standing->Clustering().Clusters),
                   "clusters");
  Published.Places(
      "rebuild: planning draw runs", Picture.Standing->TransferMetrics().DrawPlanMs, "ms");
  Published.Places(
      "rebuild: of the streams, packing them", Picture.Standing->TransferMetrics().PackingMs, "ms");
  Published.Places("restand: the geometry handed over, digested",
                   Picture.Standing->TransferMetrics().DigestValue(),
                   "");
  Published.Places(
      "rebuild: digesting what it handed over", Picture.Standing->TransferMetrics().DigestMs, "ms");
  Published.Places(
      "rebuild: and the device taking them", Picture.Standing->TransferMetrics().UploadMs, "ms");
  Published.Places(
      "rebuild: mesh admission", Picture.Standing->TransferMetrics().MeshAdmissionMs, "ms");
  Published.Places(
      "rebuild: index upload", Picture.Standing->TransferMetrics().IndexUploadMs, "ms");
  Published.Places(
      "rebuild: stream upload", Picture.Standing->TransferMetrics().StreamUploadMs, "ms");
  Published.Places(
      "rebuild: draw table upload", Picture.Standing->TransferMetrics().TableUploadMs, "ms");
  Published.Places("rebuild: residency upload attempts",
                   static_cast<double>(Picture.Device.TakeUploadAttempts()),
                   "uploads");
  Published.Places("rebuild: bytes offered for upload",
                   static_cast<double>(Picture.Device.TakeUploadBytes()),
                   "bytes");
  Published.Places("rebuild: device buffer allocation attempts",
                   static_cast<double>(Picture.Device.TakeBufferAllocationAttempts()),
                   "buffers");
  Published.Places("rebuild: staging buffer allocation attempts",
                   static_cast<double>(Picture.Device.TakeStagingAllocationAttempts()),
                   "buffers");
  Published.Places("rebuild: laying the surface", Picture.Standing->SurfaceMs(), "ms");
  Published.Places("rebuild: settling placements and lights", Picture.Standing->StandMs(), "ms");
  Published.Places("rebuild: and the streams to the device", Picture.Standing->SubmitMs(), "ms");
  Published.Places(
      "rebuild: and handing it to the device took",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count(),
      "ms");
  phaseAt = std::chrono::steady_clock::now();
  Published.Places("restand: parts the proxy then stands with",
                   static_cast<double>(Picture.Standing->PartsStanding()),
                   "parts");
  Published.Places("restand: instances it carries",
                   static_cast<double>(Picture.Standing->InstancesStanding()),
                   "instances");
  Published.Places(
      "restand: the near plane the renderer stands on", Picture.Standing->NearStanding(), "m");
  Core::ReportSubjectPlacements(Published, *Picture.Standing);
  World.GroundTiles = laid.Tiles;
  Published.Places("tiles the ring laid", static_cast<double>(laid.Tiles), "tiles");
  Published.Places("tiles it is still waiting for", static_cast<double>(laid.Pending), "tiles");
  Published.Places("tiles the stack does not hold", static_cast<double>(laid.Absent), "tiles");
  Published.Places("tiles it refused", static_cast<double>(laid.Refused), "tiles");
  Published.Places("the sun stands this high", Picture.Standing->Standing().KeyElevationDeg, "deg");
  Published.Places("and bears", Picture.Standing->Standing().KeyBearingDeg, "deg");
  Published.Places("the light that reaches the ground", Picture.Standing->MeteredLux(), "lux");
  Published.Places("and the exposure metered from it",
                   Picture.Standing->Standing().KeyFromClock ? 1.0 : 0.0,
                   "yes/no");
  Published.Places("times the terrain was rebuilt", static_cast<double>(World.Relaid), "rebuilds");
  ++World.Rebuilds;
  World.RebuildMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rebuildBegan)
          .count();
  Published.Places("and what the last rebuild took", World.RebuildMs, "ms");
  Published.Places(
      "rebuild: times the world was built WHOLE", static_cast<double>(World.Rebuilds), "rebuilds");
  Published.Places("and how often it was asked about", static_cast<double>(World.Asked), "walks");
  Published.Places(
      "levels the cascade laid", static_cast<double>(over.Zoom - laid.CoarsestZoom + 1), "levels");
  Published.Places(
      "tiles it skipped as already covered", static_cast<double>(laid.Skipped), "tiles");
  Published.Places("tiles the last rebuild laid bare", static_cast<double>(laid.Bare), "tiles");
  World.Pending = laid.Pending;
  World.Bare = laid.Bare;
  World.Wanted = laid.Tiles;
  Published.Places(
      "tiles that overlap a finer level", static_cast<double>(laid.Overlapped), "tiles");
  return true;
}
}
