#include "TemporalSequence.h"
#include <cassert>

namespace outshine::Render {
namespace {
template <int Base> float RadicalInverse(int index) noexcept {
  float result = 0.0f;
  float weight = 1.0f / static_cast<float>(Base);
  int at = index + 1;
  while (at > 0) {
    result += weight * static_cast<float>(at % Base);
    at /= Base;
    weight /= static_cast<float>(Base);
  }
  return result;
}
}

TemporalSequence::Frame::Frame(TemporalSequence &sequence, bool enabled) noexcept
    : Sequence_(sequence), Previous_(sequence.Current_) {
  assert(!Sequence_.Recording_);
  Sequence_.Recording_ = true;
  if (!enabled) { return; }
  auto &state = Sequence_.Current_;
  state.PreviousJitter = state.Jitter;
  state.Sample = (state.Sample + 1) % 8;
  state.Jitter = {{RadicalInverse<2>(state.Sample) - 0.5f, RadicalInverse<3>(state.Sample) - 0.5f}};
  state.History = state.Started;
  state.Started = true;
  state.Output = 1 - state.Output;
}

TemporalSequence::Frame::~Frame() {
  if (!Committed_) { Sequence_.Current_ = Previous_; }
  Sequence_.Recording_ = false;
}

void TemporalSequence::Frame::Commit() noexcept {
  Committed_ = true;
}

void TemporalSequence::Reset() noexcept {
  assert(!Recording_);
  Current_ = {};
}

Vec2f TemporalSequence::JitterDelta() const noexcept {
  return {{Current_.Jitter[0] - Current_.PreviousJitter[0],
           Current_.Jitter[1] - Current_.PreviousJitter[1]}};
}
}
