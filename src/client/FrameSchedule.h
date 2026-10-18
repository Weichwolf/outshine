#ifndef OUTSHINE_CLIENT_FRAMESCHEDULE_H
#define OUTSHINE_CLIENT_FRAMESCHEDULE_H

#include "FrameTiming.h"
#include <SDL3/SDL_timer.h>
#include <cstdint>

namespace outshine::Client {

class FrameSchedule {
public:
  explicit FrameSchedule(std::uint64_t startedNs, FrameTiming timing = {}) noexcept
      : StartedNs_(startedNs), Timing_(timing) {}

  template <class Clock, class Sleep>
  void Wait(std::uint64_t frame, Clock clock, Sleep sleep) const noexcept {
    const auto elapsed = clock() - StartedNs_;
    const auto deadline = Timing_.DeadlineNs(frame);
    if (elapsed < deadline) { sleep(deadline - elapsed); }
  }

  void Wait(std::uint64_t frame) const noexcept {
    Wait(frame, [] { return SDL_GetTicksNS(); }, [](std::uint64_t ns) { SDL_DelayNS(ns); });
  }

private:
  std::uint64_t StartedNs_;
  FrameTiming Timing_;
};

}
#endif
