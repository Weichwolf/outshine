#ifndef OUTSHINE_ENGINE_STREAMING_STRUCTUREBUILDTASK_H
#define OUTSHINE_ENGINE_STREAMING_STRUCTUREBUILDTASK_H

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <expected>
#include <memory>
#include <optional>

#include "HeightField.h"
#include "ArtifactStore.h"
#include "StructureBake.h"
#include "StructureMesher.h"
#include "Tasks.h"

namespace outshine {
namespace Generators {
struct PreparedStructureTile;
class PreparedBuildingAssets;
}

class StructureBuildTask {
public:
  struct ProofRequest {
    uint64_t SourceKey = 0;
    Generators::StructureSurfaceRefinementLimits Limits;
  };

  struct CacheRequest {
    std::shared_ptr<Data::ArtifactStore> Store;
    Tasks *Io = nullptr;
    std::optional<Data::TileSourceIdentity> Source;
    uint64_t SourceKey = 0;
    size_t ResidentBytesMost = 0;
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
  };

  StructureBuildTask(uint32_t tile,
                     std::unique_ptr<Generators::RawTile> raw,
                     std::shared_ptr<const Ground::HeightField> heights,
                     std::unique_ptr<Output> output,
                     std::unique_ptr<MeshScratch> scratch,
                     std::optional<ProofRequest> proof = std::nullopt,
                     std::optional<CacheRequest> cache = std::nullopt);
  StructureBuildTask(uint32_t tile,
                     std::shared_ptr<const Generators::PreparedStructureTile> base,
                     std::unique_ptr<Generators::RawTile> view,
                     std::unique_ptr<Output> output,
                     std::unique_ptr<MeshScratch> scratch);
  ~StructureBuildTask();
  StructureBuildTask(const StructureBuildTask &) = delete;
  StructureBuildTask &operator=(const StructureBuildTask &) = delete;
  StructureBuildTask(StructureBuildTask &&other) noexcept;
  StructureBuildTask &operator=(StructureBuildTask &&other) noexcept;

  void UseNativeAssets(std::shared_ptr<Generators::PreparedBuildingAssets> cache, std::string key);
  void Start(Tasks &pool, const StructureMesher &mesher);
  void Resume(Tasks &pool, const StructureMesher &mesher);
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
    return Base_.get();
  }

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
  struct Artifact;

  enum class State : uint8_t { Empty, Ready, Running, Completed };

  void Posts(Tasks &pool, const StructureMesher &mesher);

  uint32_t Tile_ = 0;
  std::unique_ptr<Generators::RawTile> Raw_;
  std::shared_ptr<const Ground::HeightField> Heights_;
  std::shared_ptr<const Generators::PreparedStructureTile> Base_;
  struct NativeProducts;
  std::unique_ptr<NativeProducts> Native_;
  std::unique_ptr<Output> Output_;
  std::unique_ptr<MeshScratch> Scratch_;
  std::unique_ptr<Generators::StructureBakeProgress> Progress_;
  std::shared_ptr<std::atomic_bool> Stopping_;
  std::unique_ptr<Comparison> Comparison_;
  std::unique_ptr<Artifact> Artifact_;
  Tasks *ActivePool_ = nullptr;
  Tasks::Handle Handle_ = Tasks::kNoTask;
  State State_ = State::Ready;
};

}
#endif
