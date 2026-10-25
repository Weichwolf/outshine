#ifndef OUTSHINE_RENDER_TEMPORAL_TEMPORALSEQUENCE_H
#define OUTSHINE_RENDER_TEMPORAL_TEMPORALSEQUENCE_H

#include "Vec2.h"
#include <cstddef>

namespace outshine::Render {
class TemporalSequence {
  struct State {
    Vec2f Jitter{};
    Vec2f PreviousJitter{};
    int Sample = 0;
    size_t Output = 0;
    bool Started = false;
    bool History = false;
  };

public:
  TemporalSequence() = default;
  TemporalSequence(const TemporalSequence &) = delete;
  TemporalSequence &operator=(const TemporalSequence &) = delete;
  TemporalSequence(TemporalSequence &&) noexcept = default;
  TemporalSequence &operator=(TemporalSequence &&) noexcept = default;
  ~TemporalSequence() = default;

  class Frame {
  public:
    Frame(TemporalSequence &sequence, bool enabled) noexcept;
    ~Frame();
    Frame(const Frame &) = delete;
    Frame &operator=(const Frame &) = delete;
    Frame(Frame &&) = delete;
    Frame &operator=(Frame &&) = delete;

    void Commit() noexcept;

  private:
    TemporalSequence &Sequence_;
    State Previous_;
    bool Committed_ = false;
  };

  [[nodiscard]] Frame Begin(bool enabled) noexcept { return {*this, enabled}; }

  void Reset() noexcept;

  [[nodiscard]] const Vec2f &Jitter() const noexcept { return Current_.Jitter; }

  [[nodiscard]] Vec2f JitterDelta() const noexcept;

  [[nodiscard]] size_t OutputIndex() const noexcept { return Current_.Output; }

  [[nodiscard]] size_t HistoryIndex() const noexcept { return 1 - Current_.Output; }

  [[nodiscard]] bool HasHistory() const noexcept { return Current_.History; }

private:
  State Current_;
  bool Recording_ = false;
};
}
#endif
