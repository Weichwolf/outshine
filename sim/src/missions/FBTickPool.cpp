#include "FBTickPool.h"

namespace FlightBox {

FBTickPool::FBTickPool(size_t threads) {
  for (size_t i = 1; i < threads; i++) Workers_.emplace_back([this] { WorkerLoop(); });
}

FBTickPool::~FBTickPool() {
  {
    std::lock_guard<std::mutex> lk(M_);
    Stop_ = true;
    Generation_++;   /* a worker parked on its generation predicate must see a change, not just Stop_ */
  }
  Start_.notify_all();
  for (auto &t : Workers_) t.join();
}

void FBTickPool::Drain() {
  for (size_t i = Next_.fetch_add(1, std::memory_order_relaxed); i < Count_;
       i = Next_.fetch_add(1, std::memory_order_relaxed))
    Job_->RunIndex(i);
}

void FBTickPool::WorkerLoop() {
  size_t seen = 0;
  std::unique_lock<std::mutex> lk(M_);
  for (;;) {
    Start_.wait(lk, [&] { return Stop_ || Generation_ != seen; });
    if (Stop_) return;
    seen = Generation_;
    /* Acquiring the mutex above published Job_/Count_; the work itself runs UNLOCKED, which is the
     * whole point — the indices share nothing. */
    lk.unlock();
    Drain();
    lk.lock();
    if (--Busy_ == 0) Done_.notify_one();
  }
}

void FBTickPool::RunTick(FBTickJob &job, size_t count) {
  if (Workers_.empty()) {   /* --threads 1: the sequential reference path, no wake-up, no atomics */
    for (size_t i = 0; i < count; i++) job.RunIndex(i);
    return;
  }
  {
    std::lock_guard<std::mutex> lk(M_);
    Job_ = &job;
    Count_ = count;
    Next_.store(0, std::memory_order_relaxed);
    Busy_ = Workers_.size();
    Generation_++;
  }
  Start_.notify_all();
  Drain();   /* the caller is a worker too — one fewer thread to wake, and it cannot idle at the barrier */
  std::unique_lock<std::mutex> lk(M_);
  Done_.wait(lk, [&] { return Busy_ == 0; });
}

} // namespace FlightBox
