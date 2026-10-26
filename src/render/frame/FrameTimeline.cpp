#include "FrameTimeline.h"
#include <cassert>

namespace outshine::Render {

void FrameTimeline::Submitted(Native::Completion completion) noexcept {
  assert(!Pending_[Next_]);
  Pending_[Next_] = completion;
  Last_ = completion;
  Next_ = (Next_ + 1) % Capacity;
}

void FrameTimeline::Retire(Native::Completion completion) noexcept {
  for (auto &pending : Pending_) {
    if (pending && pending->Device == completion.Device && pending->Serial == completion.Serial) {
      pending.reset();
    }
  }
}

void FrameTimeline::RetireAll() noexcept {
  for (auto &pending : Pending_) { pending.reset(); }
}

}
