#ifndef OUTSHINE_ENGINE_STREAMING_BUILDING_STRUCTUREBUILDTASK_H
#define OUTSHINE_ENGINE_STREAMING_BUILDING_STRUCTUREBUILDTASK_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <expected>
#include <memory>
#include <optional>

#include "HeightField.h"
#include "JobAdmission.h"
#include "StructureBake.h"
#include "StructureMesher.h"

namespace outshine {
class Tasks;

namespace Generators {
struct PreparedStructureTile;
struct PreparedBuildingBasis;
class PreparedBuildingAssets;
}

class StructureBuildTask {
public:
  struct ProofRequest {
    uint64_t SourceKey = 0;
    Generators::StructureSurfaceRefinementLimits Limits;
  };

  struct Output {
    std::optional<Generators::BakedTile> Tile;
    std::expected<void, Generators::StructureBakeError> Status;
    double BakeMs = 0.0;
    double CacheReadMs = 0.0;
    double CacheWriteMs = 0.0;
    bool CacheHit = false;
    size_t LastRanges = 0;
    size_t LastProofWork = 0;
    double LastRangeMs = 0.0;
    double FinalizationMs = 0.0;
    double LastQueueMs = 0.0;
    double LastTaskMs = 0.0;
    // Retained package capacity; raw/baked CPU products remain separate owners.
    size_t PublicationBytes = 0;
    uint64_t PublicationAttempts = 0;
  };

  StructureBuildTask(JobAdmission::Lease admission,
                     uint32_t tile,
                     std::unique_ptr<Generators::RawTile> raw,
                     std::shared_ptr<const Ground::HeightField> heights,
                     std::unique_ptr<Output> output,
                     std::unique_ptr<MeshScratch> scratch,
                     std::optional<ProofRequest> proof = std::nullopt);
  StructureBuildTask(JobAdmission::Lease admission,
                     uint32_t tile,
                     std::shared_ptr<const Generators::PreparedStructureTile> base,
                     std::unique_ptr<Generators::RawTile> view,
                     std::unique_ptr<Output> output,
                     std::unique_ptr<MeshScratch> scratch);
  StructureBuildTask(JobAdmission::Lease admission,
                     uint32_t tile,
                     std::shared_ptr<const Generators::PreparedBuildingBasis> basis,
                     std::unique_ptr<Generators::RawTile> view,
                     std::unique_ptr<Output> output,
                     std::unique_ptr<MeshScratch> scratch);
  ~StructureBuildTask();
  StructureBuildTask(const StructureBuildTask &) = delete;
  StructureBuildTask &operator=(const StructureBuildTask &) = delete;
  // Non-self moves leave the source empty; Running() remains a valid query.
  StructureBuildTask(StructureBuildTask &&other) noexcept;
  StructureBuildTask &operator=(StructureBuildTask &&other) noexcept;

  void UseNativeAssets(std::shared_ptr<Generators::PreparedBuildingAssets> cache,
                       std::string key,
                       std::string requestKey = {});
  [[nodiscard]] std::optional<std::string_view> PreparedAssetKey() const noexcept;
  void UseTerrainContacts();
  void UseInitialCells();
  [[nodiscard]] uint64_t InitialCells() const noexcept;
  void SelectNextCell(uint32_t cell);
  void Start(Tasks &pool,
             std::shared_ptr<const StructureMesher> mesher,
             std::shared_ptr<const void> lifetime = {});
  void Resume(Tasks &pool);
  [[nodiscard]] bool CanResume() const noexcept;
  [[nodiscard]] double ResumeDelaySeconds() const noexcept;
  void RequestStop() noexcept;
  [[nodiscard]] bool TakeCompletion();
  void Join();
  // Rejection retains the stopped payload; the executor must outlive this call.
  [[nodiscard]] bool Retire(Tasks &pool);

  [[nodiscard]] bool Running() const noexcept;

  [[nodiscard]] bool AwaitCompletion(double seconds) const;

  static constexpr size_t StructuresPerRange = 64;
  static constexpr size_t RangesPerTask = 4;

  [[nodiscard]] uint32_t Tile() const noexcept { return Tile_; }

  [[nodiscard]] const Generators::RawTile &Raw() const noexcept;
  [[nodiscard]] const Ground::HeightField &Heights() const noexcept;
  [[nodiscard]] const Generators::PreparedStructureTile *PreparedBase() const noexcept;
  [[nodiscard]] bool FromPreparedInputs() const noexcept;

  [[nodiscard]] size_t PreparedStructureCount() const noexcept;
  [[nodiscard]] std::span<const Data::TileSourceIdentity> HeightSources() const noexcept;
  [[nodiscard]] uint64_t HeightRasterDigest() const noexcept;
  [[nodiscard]] bool HeightQualified() const noexcept;
  [[nodiscard]] Ground::HeightField::Request HeightRequest() const;

  [[nodiscard]] Generators::RawTile &Raw() noexcept;
  [[nodiscard]] Output &Result() noexcept;
  [[nodiscard]] const Output &Result() const noexcept;
  [[nodiscard]] Generators::StructureBakeProgress &Progress() noexcept;

  [[nodiscard]] std::unique_ptr<Generators::RawTile> TakeRaw() noexcept;
  [[nodiscard]] std::unique_ptr<Output> TakeOutputForReuse() noexcept;
  [[nodiscard]] std::unique_ptr<MeshScratch> TakeScratch() noexcept;

private:
  struct Comparison;
  struct NativeProducts;
  struct Payload;

  enum class State : uint8_t { Empty, Ready, Running, Completed };

  void Posts(Tasks &pool);

  uint32_t Tile_ = 0;
  std::shared_ptr<Payload> Payload_;
  State State_ = State::Ready;
};

}
#endif
