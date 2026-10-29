#ifndef OUTSHINE_ENGINE_STREAMING_BUILDING_STRUCTUREBUILDTASK_H
#define OUTSHINE_ENGINE_STREAMING_BUILDING_STRUCTUREBUILDTASK_H

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <expected>
#include <memory>
#include <optional>

#include "HeightField.h"
#include "StructureBake.h"
#include "StructureMesher.h"
#include "Tasks.h"

namespace outshine {
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

  StructureBuildTask(uint32_t tile,
                     std::unique_ptr<Generators::RawTile> raw,
                     std::shared_ptr<const Ground::HeightField> heights,
                     std::unique_ptr<Output> output,
                     std::unique_ptr<MeshScratch> scratch,
                     std::optional<ProofRequest> proof = std::nullopt);
  StructureBuildTask(uint32_t tile,
                     std::shared_ptr<const Generators::PreparedStructureTile> base,
                     std::unique_ptr<Generators::RawTile> view,
                     std::unique_ptr<Output> output,
                     std::unique_ptr<MeshScratch> scratch);
  StructureBuildTask(uint32_t tile,
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
  void UseInitialCells();
  [[nodiscard]] uint64_t InitialCells() const noexcept;
  void SelectNextCell(uint32_t cell);
  void Start(Tasks &pool, const StructureMesher &mesher);
  void Resume(Tasks &pool, const StructureMesher &mesher);
  [[nodiscard]] bool CanResume() const noexcept;
  void RequestStop() noexcept;
  [[nodiscard]] bool TakeCompletion(Tasks &pool);
  void Join(Tasks &pool);

  [[nodiscard]] bool Running() const noexcept;

  [[nodiscard]] bool AwaitCompletion(double seconds) const;

  static constexpr size_t StructuresPerRange = 64;
  static constexpr size_t RangesPerTask = 4;

  [[nodiscard]] uint32_t Tile() const noexcept { return Tile_; }

  [[nodiscard]] const Generators::RawTile &Raw() const noexcept { return *Raw_; }

  [[nodiscard]] const Ground::HeightField &Heights() const noexcept {
    assert(Heights_ != nullptr);
    return *Heights_;
  }

  [[nodiscard]] const Generators::PreparedStructureTile *PreparedBase() const noexcept {
    return Base_ ? Base_->get() : nullptr;
  }

  [[nodiscard]] bool FromPreparedInputs() const noexcept { return (Base_ && *Base_) || Basis_; }

  [[nodiscard]] size_t PreparedStructureCount() const noexcept;
  [[nodiscard]] std::span<const Data::TileSourceIdentity> HeightSources() const noexcept;
  [[nodiscard]] uint64_t HeightRasterDigest() const noexcept;
  [[nodiscard]] bool HeightQualified() const noexcept;
  [[nodiscard]] Ground::HeightField::Request HeightRequest() const;

  [[nodiscard]] Generators::RawTile &Raw() noexcept { return *Raw_; }

  [[nodiscard]] Output &Result() noexcept { return *Output_; }

  [[nodiscard]] const Output &Result() const noexcept { return *Output_; }

  [[nodiscard]] Generators::StructureBakeProgress &Progress() noexcept { return *Progress_; }

  [[nodiscard]] std::unique_ptr<Generators::RawTile> TakeRaw() noexcept;
  [[nodiscard]] std::unique_ptr<Output> TakeOutput() noexcept;
  [[nodiscard]] std::unique_ptr<MeshScratch> TakeScratch() noexcept;

private:
  struct Comparison;

  enum class State : uint8_t { Empty, Ready, Running, Completed };

  void Posts(Tasks &pool, const StructureMesher &mesher);

  uint32_t Tile_ = 0;
  std::unique_ptr<Generators::RawTile> Raw_;
  std::shared_ptr<const Ground::HeightField> Heights_;
  using PreparedBaseOwner = std::shared_ptr<const Generators::PreparedStructureTile>;
  std::unique_ptr<PreparedBaseOwner> Base_;
  std::shared_ptr<const Generators::PreparedBuildingBasis> Basis_;
  struct NativeProducts;
  std::unique_ptr<NativeProducts> Native_;
  std::unique_ptr<Output> Output_;
  std::unique_ptr<MeshScratch> Scratch_;
  std::unique_ptr<Generators::StructureBakeProgress> Progress_;
  std::shared_ptr<std::atomic_bool> Stopping_;
  std::unique_ptr<Comparison> Comparison_;
  Tasks *ActivePool_ = nullptr;
  Tasks::Handle Handle_ = Tasks::kNoTask;
  State State_ = State::Ready;
};

}
#endif
