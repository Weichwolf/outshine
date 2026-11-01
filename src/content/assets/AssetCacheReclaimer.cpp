#include "AssetCacheReclaimer.h"
#include "AssetCacheReclamation.h"
#include "Tasks.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace outshine {
struct AssetCacheReclaimer::State {
  explicit State(std::string directory) : Directory(std::move(directory)) {}

  const std::string Directory;
  std::atomic<bool> Stop{false};
  std::atomic<Status> Phase{Status::Idle};
  std::atomic<AssetCacheError> Error{AssetCacheError::Busy};
  std::atomic<uint64_t> ChargedBytes{0}, LimitBytes{0}, Steps{0};
};

struct AssetCacheReclaimer::Execution {
  explicit Execution(std::shared_ptr<State> owner) : Owner(std::move(owner)) {}

  std::shared_ptr<State> Owner;
  std::shared_ptr<SqliteFileBudget> Budget;
  bool Ended = false;

  ~Execution() {
    if (!Ended) { Owner->Phase.store(Status::Deferred, std::memory_order_release); }
  }

  Tasks::StepResult Finish(Status phase, AssetCacheError error = AssetCacheError::Busy) {
    Budget.reset();
    Owner->Error.store(error, std::memory_order_relaxed);
    Owner->Phase.store(phase, std::memory_order_release);
    Ended = true;
    return Tasks::StepResult::Complete;
  }

  Tasks::StepResult Run() {
    if (Owner->Stop.load(std::memory_order_acquire)) { return Finish(Status::Stopped); }
    if (!Budget) {
      const auto opened = SqliteFileBudget::Open(Owner->Directory);
      if (!opened) { return Finish(Status::Failed, AssetCacheError::Storage); }
      Budget = *opened;
      Owner->LimitBytes.store(Budget->Limit(), std::memory_order_relaxed);
    }
    const auto step = AssetSql::ReclaimStep(*Budget);
    Owner->Steps.fetch_add(1, std::memory_order_relaxed);
    const auto usage = Budget->Inspect();
    if (usage) { Owner->ChargedBytes.store(usage->ChargedBytes, std::memory_order_relaxed); }
    if (!step) {
      return Finish(step.error() == AssetCacheError::Busy ? Status::Deferred : Status::Failed,
                    step.error());
    }
    if (!usage) { return Finish(Status::Failed, AssetCacheError::Storage); }
    return *step ? Finish(Status::Idle) : Tasks::StepResult::Yield;
  }
};

AssetCacheReclaimer::AssetCacheReclaimer(Tasks &workers, std::string directory)
    : Workers_(workers), State_(std::make_shared<State>(std::move(directory))) {}

AssetCacheReclaimer::~AssetCacheReclaimer() {
  State_->Stop.store(true, std::memory_order_release);
}

void AssetCacheReclaimer::Advance() {
  if (State_->Phase.load(std::memory_order_acquire) == Status::Running) { return; }
  const auto now = std::chrono::steady_clock::now();
  if (now < RetryAt_) { return; }
  RetryAt_ = now + std::chrono::seconds(1);
  State_->Phase.store(Status::Running, std::memory_order_release);
  const auto work = std::make_shared<Execution>(State_);
  if (!Workers_.PostDetachedSteps([work] { return work->Run(); })) {
    State_->Phase.store(Status::Deferred, std::memory_order_release);
  }
}

AssetCacheReclaimer::Snapshot AssetCacheReclaimer::Inspect() const noexcept {
  const auto phase = State_->Phase.load(std::memory_order_acquire);
  return {.Phase = phase,
          .Error = phase == Status::Deferred || phase == Status::Failed
                       ? std::optional(State_->Error.load(std::memory_order_relaxed))
                       : std::nullopt,
          .ChargedBytes = State_->ChargedBytes.load(std::memory_order_relaxed),
          .LimitBytes = State_->LimitBytes.load(std::memory_order_relaxed),
          .Steps = State_->Steps.load(std::memory_order_relaxed)};
}
}
