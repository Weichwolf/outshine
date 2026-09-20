#include "StructureBuildTask.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <ratio>
#include <utility>

#include "Heap.h"

namespace outshine {

StructureBuildTask::StructureBuildTask(uint32_t tile,
                                       std::unique_ptr<Generators::RawTile> raw,
                                       std::shared_ptr<const Ground::HeightField> heights,
                                       std::unique_ptr<Output> output,
                                       std::unique_ptr<MeshScratch> scratch)
    : Tile_(tile),
      Raw_(std::move(raw)),
      Heights_(std::move(heights)),
      Output_(std::move(output)),
      Scratch_(std::move(scratch)),
      Progress_(std::make_unique<Generators::StructureBakeProgress>()),
      Stopping_(std::make_shared<std::atomic_bool>(false)) {
  assert(Raw_ != nullptr && Heights_ != nullptr && Output_ != nullptr && Scratch_ != nullptr);
}

StructureBuildTask::~StructureBuildTask() {
  assert(State_ != State::Running);
}

StructureBuildTask::StructureBuildTask(StructureBuildTask &&) noexcept = default;

StructureBuildTask &StructureBuildTask::operator=(StructureBuildTask &&) noexcept = default;

bool StructureBuildTask::Running() const noexcept {
  return State_ == State::Running;
}

void StructureBuildTask::Posts(Tasks &pool, const StructureMesher &mesher) {
  assert(State_ != State::Running);
  const Generators::RawTile *const raw = Raw_.get();
  const Ground::HeightField *const heights = Heights_.get();
  MeshScratch *const scratch = Scratch_.get();
  Generators::StructureBakeProgress *const progress = Progress_.get();
  Output *const output = Output_.get();
  const std::shared_ptr<std::atomic_bool> stopping = Stopping_;
  State_ = State::Running;
  Handle_ = pool.Post([raw, heights, &mesher, scratch, progress, output, stopping] {
    const auto began = std::chrono::steady_clock::now();
    static const Heap::Tag kBakingTag("structure-bake");
    const Heap::Tagged baking(kBakingTag);
    output->LastRanges = 0;
    output->LastRangeMs = 0.0;
    for (size_t range = 0; range < RangesPerTask && !output->Tile; ++range) {
      const auto rangeBegan = std::chrono::steady_clock::now();
      const auto advanced = progress->AdvanceStructures(
          *raw, *heights, mesher, *scratch, StructuresPerRange, stopping.get());
      const double rangeMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rangeBegan)
              .count();
      output->LastRangeMs = std::max(output->LastRangeMs, rangeMs);
      if (!advanced) {
        output->Status = std::unexpected(advanced.error());
        break;
      }
      ++output->LastRanges;
      if (*advanced) {
        const auto finalizationBegan = std::chrono::steady_clock::now();
        auto finalized = progress->Finalize(*raw, mesher, *scratch, stopping.get());
        output->FinalizationMs = std::chrono::duration<double, std::milli>(
                                     std::chrono::steady_clock::now() - finalizationBegan)
                                     .count();
        if (!finalized) {
          output->Status = std::unexpected(finalized.error());
          break;
        }
        output->Tile = std::move(*finalized);
      }
    }
    const double taskMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    output->LastTaskMs = taskMs;
    output->BakeMs += taskMs;
  });
}

void StructureBuildTask::Start(Tasks &pool, const StructureMesher &mesher) {
  assert(State_ == State::Ready);
  Posts(pool, mesher);
}

void StructureBuildTask::Resume(Tasks &pool, const StructureMesher &mesher) {
  assert(State_ == State::Completed && Output_->Status && !Output_->Tile);
  Posts(pool, mesher);
}

void StructureBuildTask::RequestStop() noexcept {
  Stopping_->store(true, std::memory_order_relaxed);
}

bool StructureBuildTask::TakeCompletion(Tasks &pool) {
  if (State_ != State::Running || !pool.Done(Handle_)) { return false; }
  Handle_ = Tasks::kNoTask;
  State_ = State::Completed;
  return true;
}

void StructureBuildTask::Join(Tasks &pool) {
  if (State_ != State::Running) { return; }
  pool.Wait(Handle_);
  Handle_ = Tasks::kNoTask;
  State_ = State::Completed;
}

std::unique_ptr<Generators::RawTile> StructureBuildTask::TakeRaw() noexcept {
  assert(State_ != State::Running);
  return std::move(Raw_);
}

std::unique_ptr<StructureBuildTask::Output> StructureBuildTask::TakeOutput() noexcept {
  assert(State_ != State::Running);
  return std::move(Output_);
}

std::unique_ptr<MeshScratch> StructureBuildTask::TakeScratch() noexcept {
  assert(State_ != State::Running);
  return std::move(Scratch_);
}

}
