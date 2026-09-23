#ifndef OUTSHINE_ENGINE_STREAMING_STRUCTUREBUILDTASK_H
#define OUTSHINE_ENGINE_STREAMING_STRUCTUREBUILDTASK_H

#include <atomic>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>

#include "HeightField.h"
#include "StructureBake.h"
#include "StructureMesher.h"
#include "Tasks.h"

namespace outshine {

class StructureBuildTask {
public:
  struct Output {
    std::optional<Generators::BakedTile> Tile;
    std::expected<void, Generators::StructureBakeError> Status;
    double BakeMs = 0.0;
    size_t LastRanges = 0;
    double LastRangeMs = 0.0;
    double FinalizationMs = 0.0;
    double LastQueueMs = 0.0;
    double LastTaskMs = 0.0;
  };

  StructureBuildTask(uint32_t tile,
                     std::unique_ptr<Generators::RawTile> raw,
                     std::shared_ptr<const Ground::HeightField> heights,
                     std::unique_ptr<Output> output,
                     std::unique_ptr<MeshScratch> scratch);
  ~StructureBuildTask();
  StructureBuildTask(const StructureBuildTask &) = delete;
  StructureBuildTask &operator=(const StructureBuildTask &) = delete;
  StructureBuildTask(StructureBuildTask &&) noexcept;
  StructureBuildTask &operator=(StructureBuildTask &&) noexcept;

  void Start(Tasks &pool, const StructureMesher &mesher);
  void Resume(Tasks &pool, const StructureMesher &mesher);
  void RequestStop() noexcept;
  [[nodiscard]] bool TakeCompletion(Tasks &pool);
  void Join(Tasks &pool);

  [[nodiscard]] bool Running() const noexcept;

  static constexpr size_t StructuresPerRange = 64;
  static constexpr size_t RangesPerTask = 4;

  [[nodiscard]] uint32_t Tile() const noexcept { return Tile_; }

  [[nodiscard]] const Generators::RawTile &Raw() const noexcept { return *Raw_; }

  [[nodiscard]] const Ground::HeightField &Heights() const noexcept { return *Heights_; }

  [[nodiscard]] Generators::RawTile &Raw() noexcept { return *Raw_; }

  [[nodiscard]] Output &Result() noexcept { return *Output_; }

  [[nodiscard]] const Output &Result() const noexcept { return *Output_; }

  [[nodiscard]] Generators::StructureBakeProgress &Progress() noexcept { return *Progress_; }

  [[nodiscard]] std::unique_ptr<Generators::RawTile> TakeRaw() noexcept;
  [[nodiscard]] std::unique_ptr<Output> TakeOutput() noexcept;
  [[nodiscard]] std::unique_ptr<MeshScratch> TakeScratch() noexcept;

private:
  enum class State : uint8_t { Ready, Running, Completed };

  void Posts(Tasks &pool, const StructureMesher &mesher);

  uint32_t Tile_ = 0;
  std::unique_ptr<Generators::RawTile> Raw_;
  std::shared_ptr<const Ground::HeightField> Heights_;
  std::unique_ptr<Output> Output_;
  std::unique_ptr<MeshScratch> Scratch_;
  std::unique_ptr<Generators::StructureBakeProgress> Progress_;
  std::shared_ptr<std::atomic_bool> Stopping_;
  Tasks::Handle Handle_ = Tasks::kNoTask;
  State State_ = State::Ready;
};

}
#endif
