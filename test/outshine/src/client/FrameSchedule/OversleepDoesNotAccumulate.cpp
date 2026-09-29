#include "src/client/FrameSchedule.h"
#include "Check.h"
#include <cstdint>

int main() {
  using namespace outshine::Test;
  using namespace outshine::Client;
  constexpr std::uint64_t start = 700;
  constexpr std::uint64_t work = 3'000'000;
  constexpr std::uint64_t oversleep = 1'600'000;
  std::uint64_t now = start;
  unsigned sleeps = 0;
  FrameSchedule schedule(start);
  for (std::uint64_t frame = 0; frame < 60; ++frame) {
    schedule.Wait(
        frame,
        [&] { return now; },
        [&](std::uint64_t duration) {
          now += duration + oversleep;
          ++sleeps;
        });
    const auto expected = frame * kNanosecondsPerSecond / kFrameRateHz;
    CHECK(now - start == expected + (frame ? oversleep : 0),
          "each frame keeps its original deadline despite repeated scheduler oversleep");
    now += work;
  }
  CHECK(sleeps == 59 && now - start < kNanosecondsPerSecond,
        "sixty frames complete within one second without spinning or skipping");
  now = start + 2 * kNanosecondsPerSecond;
  schedule.Wait(60, [&] { return now; }, [&](std::uint64_t) { ++sleeps; });
  CHECK(sleeps == 59 && now - start == 2 * kNanosecondsPerSecond,
        "real overload remains late and adds no sleep");
  return Report();
}
