#include "StructureBakeTask.h"

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

namespace {

constexpr size_t kStructuresPerRange = 64;
constexpr size_t kRangesPerWorkerTask = 4;

}

StructureBakeTask::StructureBakeTask(uint32_t tile,
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

StructureBakeTask::~StructureBakeTask() {
  assert(State_ != State::Running);
}

StructureBakeTask::StructureBakeTask(StructureBakeTask &&) noexcept = default;

StructureBakeTask &StructureBakeTask::operator=(StructureBakeTask &&) noexcept = default;

bool StructureBakeTask::Running() const noexcept {
  return State_ == State::Running;
}

void StructureBakeTask::Posts(Tasks &pool, const StructureMesher &mesher) {
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
    output->LastSliceMs = 0.0;
    for (size_t range = 0; range < kRangesPerWorkerTask && !output->Complete; ++range) {
      const auto rangeBegan = std::chrono::steady_clock::now();
      const auto advanced = progress->Advance(
          *raw, *heights, mesher, *scratch, output->Tile, kStructuresPerRange, stopping.get());
      const double rangeMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rangeBegan)
              .count();
      output->LastSliceMs = std::max(output->LastSliceMs, rangeMs);
      if (!advanced) {
        output->Status = std::unexpected(advanced.error());
        break;
      }
      output->Complete = *advanced;
    }
    const double taskMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    output->LastTaskMs = taskMs;
    output->BakeMs += taskMs;
  });
}

void StructureBakeTask::Start(Tasks &pool, const StructureMesher &mesher) {
  assert(State_ == State::Ready);
  Posts(pool, mesher);
}

void StructureBakeTask::Resume(Tasks &pool, const StructureMesher &mesher) {
  assert(State_ == State::Completed && Output_->Status && !Output_->Complete);
  Posts(pool, mesher);
}

void StructureBakeTask::RequestStop() noexcept {
  Stopping_->store(true, std::memory_order_relaxed);
}

bool StructureBakeTask::TakeCompletion(Tasks &pool) {
  if (State_ != State::Running || !pool.Done(Handle_)) { return false; }
  Handle_ = Tasks::kNoTask;
  State_ = State::Completed;
  return true;
}

void StructureBakeTask::Join(Tasks &pool) {
  if (State_ != State::Running) { return; }
  pool.Wait(Handle_);
  Handle_ = Tasks::kNoTask;
  State_ = State::Completed;
}

std::unique_ptr<Generators::RawTile> StructureBakeTask::TakeRaw() noexcept {
  assert(State_ != State::Running);
  return std::move(Raw_);
}

std::unique_ptr<StructureBakeTask::Output> StructureBakeTask::TakeOutput() noexcept {
  assert(State_ != State::Running);
  return std::move(Output_);
}

std::unique_ptr<MeshScratch> StructureBakeTask::TakeScratch() noexcept {
  assert(State_ != State::Running);
  return std::move(Scratch_);
}

}
