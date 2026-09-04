#include "Tasks.h"

#include <algorithm>

namespace outshine {

namespace {

constexpr int kThreadsTheFrameKeeps = 2;
constexpr int kMostComputeThreads = 8;

} // namespace

int Tasks::ComputeThreads() {
  const auto hardware = static_cast<int>(std::thread::hardware_concurrency());
  return std::clamp(hardware - kThreadsTheFrameKeeps, 1, kMostComputeThreads);
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
  Handle which = kNoTask;
  {
    const std::scoped_lock lock(Mutex_);
    which = Next_++;
    Queue_.push_back({.Which = which, .Run = std::move(job)});
  }
  Wake_.notify_one();
  return which;
}

bool Tasks::Done(Handle which) {
  const std::scoped_lock lock(Mutex_);
  const auto at = Done_.find(which);
  if (at == Done_.end()) { return false; }
  Done_.erase(at);
  return true;
}

void Tasks::Wait(Handle which) {
  std::unique_lock<std::mutex> lock(Mutex_);
  Landed_.wait(lock, [this, which] { return Done_.contains(which); });
  Done_.erase(which);
}

size_t Tasks::Pending() {
  const std::scoped_lock lock(Mutex_);
  return Queue_.size() + Running_;
}

uint64_t Tasks::Finished() {
  const std::scoped_lock lock(Mutex_);
  return Finished_;
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
      ++Running_;
    }
    taken.Run();
    {
      const std::scoped_lock lock(Mutex_);
      --Running_;
      ++Finished_;
      Done_.insert(taken.Which);
    }
    Landed_.notify_all();
  }
}

} // namespace outshine
