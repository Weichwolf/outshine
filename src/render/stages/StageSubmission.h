#ifndef OUTSHINE_RENDER_STAGES_STAGESUBMISSION_H
#define OUTSHINE_RENDER_STAGES_STAGESUBMISSION_H

#include <array>
#include <cassert>
#include "RenderCatalogue.h"

namespace outshine::Render {

class StageSubmission;

class StageCache {
public:
  [[nodiscard]] bool Submitted() const noexcept { return State_ == State::Submitted; }

  [[nodiscard]] bool NeedsRecording() const noexcept { return State_ == State::Dirty; }

  void Invalidate() noexcept {
    assert(State_ != State::Recorded);
    State_ = State::Dirty;
  }

private:
  friend class StageSubmission;
  enum class State { Dirty, Recorded, Submitted };
  State State_ = State::Dirty;
};

class StageSubmission {
public:
  StageSubmission() = default;
  StageSubmission(const StageSubmission &) = delete;
  StageSubmission &operator=(const StageSubmission &) = delete;

  ~StageSubmission() { Finish(false); }

  void Record(Stage stage, StageCache &cache) noexcept {
    const auto slot = static_cast<size_t>(stage);
    assert(slot < Updates_.size());
    assert(Updates_[slot] == nullptr);
    assert(cache.NeedsRecording());
    Updates_[slot] = &cache;
    cache.State_ = StageCache::State::Recorded;
  }

  void Commit() noexcept { Finish(true); }

private:
  void Finish(bool accepted) noexcept {
    for (auto *&cache : Updates_) {
      if (cache == nullptr) { continue; }
      cache->State_ = accepted ? StageCache::State::Submitted : StageCache::State::Dirty;
      cache = nullptr;
    }
  }

  std::array<StageCache *, kStageCount> Updates_{};
};

}
#endif
