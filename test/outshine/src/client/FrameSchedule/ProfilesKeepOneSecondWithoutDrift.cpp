#include "src/client/FrameSchedule.h"
#include "Check.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

int main() {
  using namespace outshine::Client;
  using namespace outshine::Test;
  constexpr std::uint64_t start = 93;
  constexpr std::uint64_t workNs = 3'000'000;
  constexpr double millisecondsPerSecond = 1000.0;
  constexpr double tolerance = 1e-12;
  for (const double rate : {25.0, 30.0, 60.0}) {
    const auto timing = FrameTiming::At(rate);
    CHECK(timing.has_value(), "25/30/60 Hz profiles resolve independently of simulation");
    CHECK(std::abs(timing->BudgetMs() * rate - millisecondsPerSecond) < tolerance,
          "the frame budget is one second divided by the requested sampling rate");
    std::uint64_t now = start;
    const FrameSchedule schedule(start, *timing);
    for (std::size_t frame = 0; frame < timing->TurnFrames(); ++frame) {
      schedule.Wait(frame, [&] { return now; }, [&](auto duration) { now += duration; });
      const double scaled = static_cast<double>(now - start) * rate;
      const double target = static_cast<double>(frame) * kNanosecondsPerSecond;
      CHECK(scaled >= target && scaled < target + rate,
            "deadlines are absolute and do not accumulate rounded frame periods");
      now += workNs;
    }
    schedule.Wait(
        timing->TurnFrames(), [&] { return now; }, [&](auto duration) { now += duration; });
    CHECK(now - start == kNanosecondsPerSecond,
          "exactly the requested samples and final pacing complete a one-second turn");
    now += kNanosecondsPerSecond;
    schedule.Wait(
        timing->TurnFrames(), [&] { return now; }, [&](auto duration) { now += duration; });
    CHECK(now - start == 2 * kNanosecondsPerSecond,
          "overloaded work is neither hidden nor followed by extra waiting");
  }
  for (const double rate : {0.0,
                            -30.0,
                            std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::quiet_NaN(),
                            1'000'000'001.0}) {
    CHECK(!FrameTiming::At(rate), "invalid capture rates fail before count or deadline conversion");
  }
  const auto fractional = FrameTiming::At(29.97);
  CHECK(fractional && fractional->TurnFrames() == 30 &&
            fractional->DeadlineNs(30) > kNanosecondsPerSecond,
        "fractional rates preserve their actual cadence rather than claiming an integer rate");
  const auto slow = FrameTiming::At(0.5);
  CHECK(slow && slow->DeadlineNs(1) == 2 * kNanosecondsPerSecond,
        "sub-Hertz captures keep their representable frame interval");
  return Report();
}
