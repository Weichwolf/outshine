#include "Tasks.h"

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
  return Post(std::move(job), true);
}

bool Tasks::PostDetached(Job job) {
  return Post(std::move(job), false) != kNoTask;
}

Tasks::Handle Tasks::Post(Job job, bool tracked) {
  Handle which = kNoTask;
  {
    const std::scoped_lock lock(Mutex_);
    if (Stopping_) { return kNoTask; }
    which = Next_++;
    Queue_.push_back({.Which = which, .Run = std::move(job), .Tracked = tracked});
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
    taken.Run();
    {
      const std::scoped_lock lock(Mutex_);
      if (taken.Tracked) { Done_.insert(taken.Which); }
    }
    Landed_.notify_all();
  }
}

}
