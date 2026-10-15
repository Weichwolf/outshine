#ifndef OUTSHINE_CLIENT_FRAMESCHEDULE_H
#define OUTSHINE_CLIENT_FRAMESCHEDULE_H

#include "FramePacer.h"
#include <cassert>
#include <cstdint>

namespace outshine::Client {

class FrameSchedule {
public:
  explicit FrameSchedule(std::uint64_t startedNs, std::uint64_t rateHz = kFrameRateHz) noexcept
      : StartedNs_(startedNs), RateHz_(rateHz) {
    assert(rateHz > 0);
  }

  template <class Clock, class Sleep>
  void Wait(std::uint64_t frame, Clock clock, Sleep sleep) const noexcept {
    const auto elapsed = clock() - StartedNs_;
    const auto deadline = frame * kNanosecondsPerSecond / RateHz_;
    if (elapsed < deadline) { sleep(deadline - elapsed); }
  }

  void Wait(std::uint64_t frame) const noexcept {
    Wait(frame, [] { return SDL_GetTicksNS(); }, [](std::uint64_t ns) { SDL_DelayNS(ns); });
  }

private:
  std::uint64_t StartedNs_;
  std::uint64_t RateHz_;
};

}
#endif
