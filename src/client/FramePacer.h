#ifndef OUTSHINE_CLIENT_FRAMEPACER_H
#define OUTSHINE_CLIENT_FRAMEPACER_H

#include <SDL3/SDL_timer.h>
#include <cstdint>
#include <optional>

namespace outshine::Client {

class FramePacer {
public:
  explicit FramePacer(std::uint64_t periodNs = 16'666'667) noexcept : PeriodNs_(periodNs) {}

  template <class Clock, class Sleep> void Wait(Clock clock, Sleep sleep) noexcept {
    auto now = clock();
    if (StartedNs_ && now - *StartedNs_ < PeriodNs_) {
      sleep(PeriodNs_ - (now - *StartedNs_));
      now = clock();
    }
    StartedNs_ = now;
  }

  void Wait() noexcept {
    Wait([] { return SDL_GetTicksNS(); }, [](std::uint64_t ns) { SDL_DelayNS(ns); });
  }

private:
  std::uint64_t PeriodNs_;
  std::optional<std::uint64_t> StartedNs_;
};

}
#endif
