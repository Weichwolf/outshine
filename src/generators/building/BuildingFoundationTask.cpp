#include "BuildingFoundationTask.h"
#include "BuildingFoundationJob.h"
#include "Tasks.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ratio>
#include <utility>
#include <vector>

namespace outshine::Generators {
struct BuildingFoundationTask::State {
  static constexpr size_t kWorkPerSlice = 2048;

  State(TangentFrame frame,
        uint64_t generation,
        std::vector<::outshine::Ground::BuildingProductLease> products)
      : Job(frame, generation, std::move(products)),
        Work{.Generation = generation, .UnitsMost = kWorkPerSlice},
        Bytes(Job.HeapBytes()) {}

  Tasks::StepResult Step() {
    if (Stopping.load(std::memory_order_relaxed)) {
      return Finish(std::unexpected("building foundation preparation cancelled"));
    }
    const auto began = std::chrono::steady_clock::now();
    const auto advanced = Job.Advance(Work);
    WorkerMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    Bytes.store(Job.HeapBytes(), std::memory_order_relaxed);
    Visited.store(Job.VisitedFootprints(), std::memory_order_relaxed);
    if (!advanced) { return Finish(std::unexpected(advanced.error())); }
    if (!*advanced) { return Tasks::StepResult::Yield; }
    auto stamps = Job.Take();
    if (!stamps) { return Finish(std::unexpected(stamps.error())); }
    return Finish(Product{.Stamps = std::move(*stamps), .WorkerMs = WorkerMs});
  }

  Tasks::StepResult Finish(Result result) {
    Output.emplace(std::move(result));
    Finished.store(true, std::memory_order_release);
    return Tasks::StepResult::Complete;
  }

  BuildingFoundationJob Job;
  BuildingFoundationJob::Work Work;
  std::atomic_size_t Bytes;
  std::atomic_size_t Visited{0};
  std::atomic_bool Stopping{false};
  std::atomic_bool Finished{false};
  double WorkerMs = 0;
  std::optional<Result> Output;
};

BuildingFoundationTask::BuildingFoundationTask(
    Tasks &pool,
    TangentFrame frame,
    uint64_t generation,
    std::vector<::outshine::Ground::BuildingProductLease> products)
    : Pool_(pool), State_(std::make_shared<State>(frame, generation, std::move(products))) {
  if (!Pool_.PostDetachedSteps([state = State_] { return state->Step(); })) {
    (void)State_->Finish(
        std::unexpected("compute worker rejected building foundation preparation"));
  }
}

BuildingFoundationTask::~BuildingFoundationTask() {
  State_->Stopping.store(true, std::memory_order_relaxed);
  (void)Pool_.PostDetached([state = std::move(State_)] { (void)state; });
}

std::optional<BuildingFoundationTask::Result> BuildingFoundationTask::Take() {
  if (!State_->Finished.load(std::memory_order_acquire)) { return std::nullopt; }
  return std::exchange(State_->Output, std::nullopt);
}

bool BuildingFoundationTask::AwaitSlice(double seconds) const {
  const auto epoch = Pool_.CompletionEpoch();
  return State_->Finished.load(std::memory_order_acquire) || Pool_.AwaitProgress(epoch, seconds);
}

size_t BuildingFoundationTask::VisitedFootprints() const noexcept {
  return State_->Visited.load(std::memory_order_relaxed);
}

size_t BuildingFoundationTask::HeapBytes() const noexcept {
  return sizeof(State) + State_->Bytes.load(std::memory_order_relaxed);
}
}
