#include "Graph.h"

namespace outshine::Work {

Graph::Graph(unsigned hands) : Hands_(hands == 0 ? 1u : hands) {
  Hand_.reserve(Hands_);
  for (unsigned at = 0; at + 1u < Hands_; ++at) {
    Hand_.emplace_back([this]() { Serves(false); });
  }
}

Graph::~Graph() {
  {
    std::lock_guard<std::mutex> held(Guard_);
    Closing_ = true;
  }
  Woke_.notify_all();
  for (std::thread &hand : Hand_) {
    if (hand.joinable()) { hand.join(); }
  }
}

void Graph::Clears() {
  Steps_ = 0;
  Error_.clear();
  for (size_t at = 0; at < kMostSteps; ++at) {
    Held_[at].Act = nullptr;
    Held_[at].Afters = 0;
    Held_[at].Fed = 0;
    Held_[at].Owed.store(0, std::memory_order_relaxed);
  }
}

uint32_t Graph::Adds(Does does, void *with) {
  if (Steps_ >= kMostSteps || does == nullptr) {
    Error_ = "a frame declares more steps than the graph holds, or a step that does nothing -- "
             "the capacity is fixed so a frame allocates none of it";
    return 0xFFFFFFFFu;
  }
  Step &one = Held_[Steps_];
  one.Act = does;
  one.With = with;
  return static_cast<uint32_t>(Steps_++);
}

bool Graph::After(uint32_t step, uint32_t earlier) {
  if (step >= Steps_ || earlier >= Steps_ || step == earlier) {
    Error_ = "a step waits on one the graph does not hold, or on itself";
    return false;
  }
  Step &later = Held_[step];
  Step &before = Held_[earlier];
  if (later.Afters >= kMostAfter || before.Fed >= kMostAfter) {
    Error_ = "a step waits on more than the graph holds";
    return false;
  }
  later.After[later.Afters++] = earlier;
  before.Feeds[before.Fed++] = step;
  return true;
}

void Graph::Serves(bool untilDone) {
  for (;;) {
    uint32_t next = 0xFFFFFFFFu;
    {
      std::unique_lock<std::mutex> held(Guard_);
      Woke_.wait(held, [this, untilDone]() {
        return Closing_ || !Ready_.empty() ||
               (untilDone && Left_.load(std::memory_order_acquire) == 0);
      });
      if (untilDone && Left_.load(std::memory_order_acquire) == 0) { return; }
      if (Closing_ && Ready_.empty()) { return; }
      next = Ready_.back();
      Ready_.pop_back();
    }
    Step &one = Held_[next];
    one.Act(one.With);
    std::vector<uint32_t> freed;
    for (uint8_t at = 0; at < one.Fed; ++at) {
      Step &after = Held_[one.Feeds[at]];
      if (after.Owed.fetch_sub(1u, std::memory_order_acq_rel) == 1u) {
        freed.push_back(one.Feeds[at]);
      }
    }
    {
      std::lock_guard<std::mutex> held(Guard_);
      for (const uint32_t one2 : freed) { Ready_.push_back(one2); }
      Left_.fetch_sub(1u, std::memory_order_acq_rel);
    }
    Woke_.notify_all();
  }
}

bool Graph::Runs() {
  if (Steps_ == 0) { return true; }
  Left_.store(Steps_, std::memory_order_relaxed);
  {
    std::lock_guard<std::mutex> held(Guard_);
    Ready_.clear();
    for (size_t at = 0; at < Steps_; ++at) {
      Held_[at].Owed.store(Held_[at].Afters, std::memory_order_relaxed);
    }
    for (size_t at = 0; at < Steps_; ++at) {
      if (Held_[at].Afters == 0) { Ready_.push_back(static_cast<uint32_t>(at)); }
    }
    if (Ready_.empty()) {
      Error_ = "every step waits on another, so the graph holds a cycle and nothing can start";
      return false;
    }
  }
  Woke_.notify_all();
  Serves(true);
  return true;
}

} // namespace outshine::Work
