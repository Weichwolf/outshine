#include "Tasks.h"
#include <variant>

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <utility>

namespace outshine {

int Tasks::ComputeThreads() {
  return 1;
}

Tasks::Tasks(int threads) {
  const int n = threads > 0 ? threads : 1;
  Threads_.reserve(static_cast<size_t>(n));
  for (int at = 0; at < n; ++at) {
    Threads_.emplace_back([this] { Work(); });
  }
}

Tasks::~Tasks() {
  {
    const std::scoped_lock lock(Mutex_);
    Stopping_ = true;
    Queue_.clear();
  }
  Wake_.notify_all();
  for (std::thread &one : Threads_) { one.join(); }
}

Tasks::Handle Tasks::Post(Job job) {
  return Queue(WorkItem(std::in_place_type<Job>, std::move(job)), true);
}

Tasks::Handle Tasks::PostSteps(Step step) {
  return Queue(WorkItem(std::in_place_type<Step>, std::move(step)), true);
}

bool Tasks::PostDetached(Job job) {
  return Queue(WorkItem(std::in_place_type<Job>, std::move(job)), false) != kNoTask;
}

bool Tasks::PostDetachedSteps(Step step) {
  return Queue(WorkItem(std::in_place_type<Step>, std::move(step)), false) != kNoTask;
}

Tasks::Handle Tasks::Queue(WorkItem work, bool tracked) {
  Handle which = kNoTask;
  {
    const std::scoped_lock lock(Mutex_);
    if (Stopping_) { return kNoTask; }
    which = Next_++;
    Queue_.push_back({.Which = which, .Run = std::move(work), .Tracked = tracked});
  }
  Wake_.notify_one();
  return which;
}

bool Tasks::TakeCompletion(Handle which) {
  const std::scoped_lock lock(Mutex_);
  const auto at = Done_.find(which);
  if (at == Done_.end()) { return false; }
  Done_.erase(at);
  return true;
}

bool Tasks::AwaitCompletion(double seconds) {
  if (!(seconds > 0.0)) { return false; }
  std::unique_lock<std::mutex> lock(Mutex_);
  return Landed_.wait_for(
      lock, std::chrono::duration<double>(seconds), [this] { return !Done_.empty(); });
}

bool Tasks::AwaitCompletion(Handle which, double seconds) {
  if (which == kNoTask || !(seconds > 0.0)) { return false; }
  std::unique_lock<std::mutex> lock(Mutex_);
  return Landed_.wait_for(lock, std::chrono::duration<double>(seconds), [this, which] {
    return Done_.contains(which);
  });
}

void Tasks::Wait(Handle which) {
  std::unique_lock<std::mutex> lock(Mutex_);
  Landed_.wait(lock, [this, which] { return Done_.contains(which); });
  Done_.erase(which);
}

void Tasks::Work() {
  for (;;) {
    Posted taken;
    {
      std::unique_lock<std::mutex> lock(Mutex_);
      Wake_.wait(lock, [this] { return Stopping_ || !Queue_.empty(); });
      if (Stopping_) { return; }
      taken = std::move(Queue_.front());
      Queue_.pop_front();
    }
    StepResult result = StepResult::Complete;
    if (const auto *job = std::get_if<Job>(&taken.Run)) {
      (*job)();
    } else {
      result = std::get<Step>(taken.Run)();
    }
    {
      const std::scoped_lock lock(Mutex_);
      if (result == StepResult::Yield) {
        if (!Stopping_) { Queue_.push_back(std::move(taken)); }
        Wake_.notify_one();
        continue;
      }
      if (taken.Tracked) { Done_.insert(taken.Which); }
    }
    Landed_.notify_all();
  }
}

}
