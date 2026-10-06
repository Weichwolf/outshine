#ifndef OUTSHINE_ENGINE_STREAMING_STRUCTUREBUILDQUEUE_H
#define OUTSHINE_ENGINE_STREAMING_STRUCTUREBUILDQUEUE_H

#include <scene/ProjectedErrorBudget.h>
#include <array>
#include <expected>
#include <functional>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "math/Vec3.h"

#include "HeightField.h"
#include "OsmStructurePreparation.h"
#include "SurfacePreparation.h"
#include "StructureBake.h"
#include "StructureBuildTask.h"
#include "StructureSourcePreparation.h"
#include "StructureMesher.h"
#include "Tasks.h"

namespace outshine {

class StructureBuildQueue {
public:
  static constexpr size_t kCandidateWindow = 4;
  enum class HeightRequirement : uint8_t { AllowFallback, FineOnly };
  enum class CellSourceState : uint8_t { Current, Unknown, Stale, ScopeChanged, Pending };
  enum class BuildPurpose : uint8_t { ViewDetail, SourceGeometry };

  struct HeightSourceRevision {
    uint64_t Value = 0;
    friend bool operator==(HeightSourceRevision, HeightSourceRevision) = default;
  };

  ~StructureBuildQueue();

  void Opens(Tasks *pool, const StructureMesher *mesher) {
    Pool_ = pool;
    Mesher_ = mesher;
  }

  [[nodiscard]] std::expected<bool, std::string>
  PrepareOriginal(std::shared_ptr<const Generators::Osm::SourceSnapshot> source,
                  Generators::Osm::StructurePolicy policy,
                  int heightZoom);
  [[nodiscard]] std::expected<bool, std::string>
  PrepareOriginal(std::span<const std::shared_ptr<const Generators::Osm::SourceSnapshot>> sources,
                  Generators::Osm::StructurePolicy policy,
                  int heightZoom);
  [[nodiscard]] std::expected<bool, std::string>
  PrepareOriginal(std::span<const std::shared_ptr<const Generators::Osm::StructureCell>> cells,
                  int heightZoom);
  [[nodiscard]] std::expected<std::span<const Data::TileId>, std::string>
  OriginalHeightTiles(int zoom) const;

  [[nodiscard]] bool HasOriginal() const noexcept { return !Originals_.empty(); }

  struct BakeRevision {
    uint64_t Vectors = 0;
    HeightSourceRevision HeightSource;
    ProjectedErrorBudget Projection{};
    double TileSpanM = 0.0;
    LongitudeLatitude Eye;
    std::optional<Vec3> EyeEcef;
    std::optional<LevelOfDetail> RequestedDetail;
    BuildPurpose Purpose = BuildPurpose::ViewDetail;
    bool FallbackHeights = false;
    const Data::SourceObjects *InputObjects = nullptr;

    [[nodiscard]] bool Matches(const ::outshine::Generators::Osm::OsmField *vectors,
                               const ::outshine::Generators::Osm::BuildingField &footprints,
                               LongitudeLatitude eye,
                               HeightSourceRevision heightSource,
                               HeightRequirement heights = HeightRequirement::AllowFallback,
                               std::optional<LevelOfDetail> detail = std::nullopt,
                               BuildPurpose purpose = BuildPurpose::ViewDetail,
                               const Data::SourceObjects *inputObjects = nullptr) const noexcept;

    [[nodiscard]] bool Matches(const ::outshine::Generators::Osm::OsmField &vectors,
                               const ::outshine::Generators::Osm::BuildingField &footprints,
                               LongitudeLatitude eye,
                               HeightSourceRevision heightSource,
                               HeightRequirement heights = HeightRequirement::AllowFallback,
                               std::optional<LevelOfDetail> detail = std::nullopt,
                               BuildPurpose purpose = BuildPurpose::ViewDetail) const noexcept {
      return Matches(&vectors, footprints, eye, heightSource, heights, detail, purpose);
    }

    [[nodiscard]] bool
    OwnsReservation(const ::outshine::Generators::Osm::OsmField *vectors,
                    const ::outshine::Generators::Osm::BuildingField &footprints,
                    LongitudeLatitude eye,
                    HeightSourceRevision heightSource,
                    const Data::SourceObjects *inputObjects = nullptr) const noexcept {
      (void)eye;
      return (InputObjects != nullptr ? InputObjects == inputObjects
                                      : vectors != nullptr && Vectors == vectors->Generation()) &&
             HeightSource == heightSource &&
             ((RequestedDetail && *RequestedDetail != LevelOfDetail::Fine) ||
              Purpose == BuildPurpose::SourceGeometry || Projection == footprints.Projection()) &&
             TileSpanM == footprints.TileSpanM();
    }

    [[nodiscard]] bool OwnsReservation(const ::outshine::Generators::Osm::OsmField &vectors,
                                       const ::outshine::Generators::Osm::BuildingField &footprints,
                                       LongitudeLatitude eye,
                                       HeightSourceRevision heightSource) const noexcept {
      return OwnsReservation(&vectors, footprints, eye, heightSource);
    }
  };

  struct HeightFailure {
    enum class Reason { None, Unavailable, Unqualified };
    Reason Cause = Reason::None;
    Data::TileId Tile{};
  };

  struct HeightSource {
    std::function<std::optional<double>(LongitudeLatitude)> Sample;
    std::function<bool(Data::TileId, Ground::HeightField::Block &)> PinField;
    std::function<std::shared_ptr<const Ground::TerrainField>(Data::TileId)> ResidentField;
    std::function<bool(Data::TileId, Ground::HeightField::Block &)> PinResidentField = nullptr;
    HeightSourceRevision Revision;
    std::function<std::expected<SourcedTerrainFields, SourcedTerrainFields::CaptureError>(
        std::span<const Ground::TileSpot>, size_t)>
        CaptureFields = nullptr;
  };

  [[nodiscard]] size_t Posts(Ground::SurfacePreparation &stack,
                             ::outshine::Generators::Osm::BuildingField &footprints,
                             LongitudeLatitude eye,
                             const HeightSource &heightAt,
                             size_t candidatesMost,
                             HeightRequirement requirement = HeightRequirement::AllowFallback,
                             std::optional<LevelOfDetail> detail = std::nullopt,
                             BuildPurpose purpose = BuildPurpose::ViewDetail,
                             const std::function<bool(uint32_t)> &cellReady = {});

  struct CellRequest {
    uint32_t Tile = 0;
    uint32_t Cell = 0;
    LevelOfDetail Detail = LevelOfDetail::Massed;
    uint64_t SourceKey = 0;
  };

  [[nodiscard]] static std::optional<uint64_t>
  QualifiedSourceKey(const ::outshine::Generators::Osm::BuildingField &footprints, uint32_t tile);
  [[nodiscard]] static CellSourceState
  InspectCellSource(const Ground::SurfacePreparation &stack,
                    const ::outshine::Generators::Osm::BuildingField &footprints,
                    const HeightSource &heightAt,
                    uint32_t tile,
                    uint64_t sourceKey);
  [[nodiscard]] static bool
  ValidateResidentCellSource(const Ground::SurfacePreparation &stack,
                             const ::outshine::Generators::Osm::BuildingField &footprints,
                             const HeightSource &heightAt,
                             uint32_t tile,
                             uint64_t sourceKey);
  [[nodiscard]] bool PostsCell(Ground::SurfacePreparation &stack,
                               ::outshine::Generators::Osm::BuildingField &footprints,
                               LongitudeLatitude eye,
                               const HeightSource &heightAt,
                               CellRequest request);

  [[nodiscard]] std::expected<size_t, std::string>
  PostsCells(Ground::SurfacePreparation &stack,
             ::outshine::Generators::Osm::BuildingField &footprints,
             LongitudeLatitude eye,
             const HeightSource &heightAt,
             std::span<const CellRequest> requests);

  struct Landing {
    uint32_t Tile = 0;
    const Generators::BakedTile *Baked = nullptr;
    Vec3 AnchorEcef;
    uint64_t SourceKey = 0;
    std::optional<::outshine::Generators::Osm::BuildingField::PendingAcceptance> Footprints;
  };

  [[nodiscard]] std::expected<std::vector<Landing>, Generators::StructureBakeError>
  NextLandings(Ground::SurfacePreparation &stack,
               ::outshine::Generators::Osm::BuildingField &footprints,
               LongitudeLatitude eye,
               const HeightSource &heightAt,
               size_t most,
               HeightRequirement heights = HeightRequirement::AllowFallback,
               std::optional<LevelOfDetail> detail = std::nullopt,
               BuildPurpose purpose = BuildPurpose::ViewDetail);
  [[nodiscard]] std::expected<std::optional<Landing>, Generators::StructureBakeError>
  NextCellLanding(const Ground::SurfacePreparation &stack,
                  const ::outshine::Generators::Osm::BuildingField &footprints,
                  const HeightSource &heightAt);
  void CommitsCellLanding(const Landing &landing) noexcept;
  void ResumeCompletedTasks();
  void CommitsLandings(Ground::SurfacePreparation &stack,
                       ::outshine::Generators::Osm::BuildingField &footprints,
                       std::span<Landing> landings) noexcept;
  void Clear();

  [[nodiscard]] size_t Queued() const {
    return Queue_.size() +
           static_cast<size_t>(OriginalPreparation_ && OriginalPreparation_->Running());
  }

  [[nodiscard]] size_t QueuedCells() const;

  [[nodiscard]] size_t FastCellValidations() const noexcept { return FastCellValidations_; }

  [[nodiscard]] bool CellQueued(CellRequest request) const noexcept;

  [[nodiscard]] bool Complete(const Ground::SurfacePreparation &stack,
                              const ::outshine::Generators::Osm::BuildingField &footprints,
                              LongitudeLatitude eye,
                              const std::function<bool(uint32_t)> &cellReady = {}) const;
  [[nodiscard]] bool
  SourcesComplete(const Ground::SurfacePreparation &stack,
                  const ::outshine::Generators::Osm::BuildingField &footprints) const;
  [[nodiscard]] static bool
  QualifiedSources(const Ground::SurfacePreparation &stack,
                   const ::outshine::Generators::Osm::BuildingField &footprints);

  [[nodiscard]] size_t Posted() const { return Posted_; }

  [[nodiscard]] size_t Landed() const { return Landed_; }

  [[nodiscard]] size_t Deferred() const { return Deferred_; }

  [[nodiscard]] HeightFailure LastHeightFailure() const noexcept { return LastHeightFailure_; }

  [[nodiscard]] size_t Discarded() const { return Discarded_; }

  [[nodiscard]] double BakeWorkMs() const noexcept { return BakedMs_; }

  [[nodiscard]] double MeanBakeMs() const {
    return Landed_ > 0 ? BakedMs_ / static_cast<double>(Landed_) : 0.0;
  }

  [[nodiscard]] double SlowestBakeMs() const { return SlowestBakeMs_; }

  [[nodiscard]] size_t CompletedRanges() const { return CompletedRanges_; }

  [[nodiscard]] double SlowestRangeMs() const { return SlowestRangeMs_; }

  [[nodiscard]] double SlowestFinalizationMs() const { return SlowestFinalizationMs_; }

  [[nodiscard]] double SlowestTaskMs() const { return SlowestTaskMs_; }

  [[nodiscard]] double SlowestQueueMs() const { return SlowestQueueMs_; }

  [[nodiscard]] double SlowestCandidateSelectionMs() const { return SlowestCandidateSelectionMs_; }

  [[nodiscard]] double SlowestHeightResolutionMs() const { return SlowestHeightResolutionMs_; }

  [[nodiscard]] double SlowestRawExtractionMs() const { return SlowestRawExtractionMs_; }

  [[nodiscard]] double SlowestTaskPostingMs() const { return SlowestTaskPostingMs_; }

  [[nodiscard]] size_t QueuedStructures() const;

  [[nodiscard]] bool AwaitSlice(double seconds) const {
    if (Pool_ == nullptr) { return false; }
    if (OriginalPreparation_ && OriginalPreparation_->Running()) {
      return OriginalPreparation_->AwaitSlice(seconds);
    }
    for (const auto &batch : PreparedCells_) {
      if (batch && batch->Preparation->Running()) { return Pool_->AwaitCompletion(seconds); }
    }
    for (const QueuedBuild &build : Queue_) {
      if (build.Task.Running()) { return build.Task.AwaitCompletion(seconds); }
    }
    for (const QueuedBuild &build : CellQueue_) {
      if (build.Task.Running()) { return build.Task.AwaitCompletion(seconds); }
    }
    return false;
  }

private:
  [[nodiscard]] size_t PostsVectors(Ground::SurfacePreparation &stack,
                                    ::outshine::Generators::Osm::BuildingField &prints,
                                    LongitudeLatitude eye,
                                    const HeightSource &heightAt,
                                    size_t candidatesMost,
                                    HeightRequirement requirement,
                                    std::optional<LevelOfDetail> detail,
                                    BuildPurpose purpose,
                                    const std::function<bool(uint32_t)> &cellReady);
  [[nodiscard]] size_t PostsOriginal(uint32_t tile,
                                     Ground::SurfacePreparation &stack,
                                     ::outshine::Generators::Osm::BuildingField &footprints,
                                     LongitudeLatitude eye,
                                     const HeightSource &heightAt,
                                     HeightRequirement requirement,
                                     std::optional<LevelOfDetail> detail,
                                     BuildPurpose purpose);

  struct QueuedBuild {
    BakeRevision Revision;
    StructureBuildTask Task;
    uint64_t StreetDigest = 0;
    uint64_t SourceKey = 0;
    uint32_t Cell = 0;
    size_t BakedStructures = 0;
    size_t Tasks = 0;
    bool Finished = false;
    bool Replacement = false;
    std::shared_ptr<const void> ReservationOwner = nullptr;
  };

  [[nodiscard]] static Landing PrepareLanding(QueuedBuild &bake,
                                              ::outshine::Generators::Osm::BuildingField &prints,
                                              const ::outshine::Generators::Osm::OsmField *vectors);

  template <typename T>
  [[nodiscard]] static std::unique_ptr<T> Borrowed(std::vector<std::unique_ptr<T>> &idle) {
    if (idle.empty()) { return std::make_unique<T>(); }
    std::unique_ptr<T> one = std::move(idle.back());
    idle.pop_back();
    return one;
  }

  [[nodiscard]] std::unique_ptr<MeshScratch> LentScratch();
  void RecycleOutput(StructureBuildTask &task);
  void PrepareViewRefinement(::outshine::Generators::Osm::BuildingField &prints,
                             LongitudeLatitude eye,
                             HeightRequirement requirement,
                             BuildPurpose purpose,
                             const std::function<bool(uint32_t)> &cellReady);
  void PostSlice(QueuedBuild &build);
  [[nodiscard]] bool WholeTileSourceCurrent(const Ground::SurfacePreparation &stack,
                                            const HeightSource &heightAt,
                                            const QueuedBuild &bake,
                                            HeightRequirement heights) const;
  void DiscardFront(::outshine::Generators::Osm::BuildingField &prints);
  void DiscardStale(const ::outshine::Generators::Osm::OsmField *vectors,
                    ::outshine::Generators::Osm::BuildingField &prints,
                    LongitudeLatitude eye,
                    HeightSourceRevision heightSource,
                    HeightRequirement heights,
                    std::optional<LevelOfDetail> detail,
                    BuildPurpose purpose);

  struct PinnedCellHeight {
    uint32_t Tile = 0;
    uint64_t SourceKey = 0;
    HeightSourceRevision Revision;
    std::shared_ptr<const Ground::HeightField> Heights;
  };

  struct PreparedCells {
    ::outshine::Generators::Osm::BuildingField::AcceptedInput Receipt;
    std::array<CellRequest, 8> Requests{};
    size_t Count = 0;
    size_t Next = 0;
    uint64_t Generation = 0;
    HeightSourceRevision Revision;
    LongitudeLatitude Eye;
    std::unique_ptr<StructureSourcePreparation> Preparation;
    bool Revoked = false;
  };

  [[nodiscard]] std::expected<void, Generators::StructureBakeError>
  AdvancePreparedCell(std::shared_ptr<PreparedCells> &batch,
                      const Ground::SurfacePreparation &stack,
                      const ::outshine::Generators::Osm::BuildingField &footprints,
                      const HeightSource &heightAt);
  [[nodiscard]] bool
  ValidateCellLandingSource(const Ground::SurfacePreparation &stack,
                            const ::outshine::Generators::Osm::BuildingField &footprints,
                            const HeightSource &heightAt,
                            const QueuedBuild &bake);
  void RetireCellBuilds(BuildPurpose purpose);
  [[nodiscard]] std::expected<void, Generators::StructureBakeError>
  AdvancePreparedCells(const Ground::SurfacePreparation &stack,
                       const ::outshine::Generators::Osm::BuildingField &footprints,
                       const HeightSource &heightAt);
  [[nodiscard]] bool PostPreparedCell(const Ground::SurfacePreparation &stack,
                                      const ::outshine::Generators::Osm::BuildingField &footprints,
                                      LongitudeLatitude eye,
                                      const HeightSource &heightAt,
                                      CellRequest request,
                                      std::shared_ptr<const Ground::HeightField> heights,
                                      std::shared_ptr<const void> owner);

  std::array<std::shared_ptr<PreparedCells>, kCandidateWindow> PreparedCells_{};
  Tasks *Pool_ = nullptr;
  std::unique_ptr<Generators::Osm::StructurePreparation> OriginalPreparation_;
  std::vector<std::shared_ptr<const void>> PreparingOriginals_;
  [[nodiscard]] std::expected<bool, std::string>
  PrepareOriginalInputs(Generators::Osm::StructurePreparation::Inputs inputs,
                        Generators::Osm::StructurePolicy policy,
                        int heightZoom);
  std::vector<Generators::Osm::StructurePreparation::Product> Originals_;
  [[nodiscard]] const Data::SourceObjects *InputObjectsFor(uint32_t tile) const noexcept;
  std::vector<Data::TileId> OriginalHeightTiles_;
  int OriginalHeightZoom_ = -1;
  int PreparingOriginalHeightZoom_ = -1;
  const StructureMesher *Mesher_ = nullptr;
  std::deque<QueuedBuild> Queue_;
  std::deque<QueuedBuild> CellQueue_;
  std::vector<std::unique_ptr<Generators::RawTile>> IdleRaw_;
  std::vector<std::unique_ptr<StructureBuildTask::Output>> IdleOut_;
  std::vector<std::unique_ptr<MeshScratch>> IdleScratch_;
  std::optional<PinnedCellHeight> PinnedCellHeight_;
  size_t FastCellValidations_ = 0;
  size_t Posted_ = 0;
  size_t Landed_ = 0;
  size_t Deferred_ = 0;
  HeightFailure LastHeightFailure_;
  size_t Discarded_ = 0;
  double BakedMs_ = 0.0;
  double SlowestBakeMs_ = 0.0;
  size_t CompletedRanges_ = 0;
  double SlowestRangeMs_ = 0.0;
  double SlowestFinalizationMs_ = 0.0;
  double SlowestQueueMs_ = 0.0;
  double SlowestTaskMs_ = 0.0;
  double SlowestCandidateSelectionMs_ = 0.0;
  double SlowestHeightResolutionMs_ = 0.0;
  double SlowestRawExtractionMs_ = 0.0;
  double SlowestTaskPostingMs_ = 0.0;
};

}
#endif
