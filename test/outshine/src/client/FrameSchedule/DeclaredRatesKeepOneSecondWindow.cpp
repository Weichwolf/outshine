#include "src/client/FrameSchedule.h"
#include "src/client/PlaceFrameProfile.h"
#include "Check.h"
#include <array>
#include <cstdint>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::array<double, 3> rates{25.0, 30.0, 60.0};
  constexpr std::uint64_t start = 900;
  constexpr std::uint64_t work = 3'000'000;
  for (const double rate : rates) {
    const auto profile = Shots::FrameProfile::ForPlace(rate);
    CHECK(profile.has_value(), "declared measurement profile is valid");
    if (!profile) { continue; }
    Client::FrameSchedule schedule(start, profile->RateHz());
    std::uint64_t now = start;
    std::size_t samples = 0;
    for (std::uint64_t frame = 0; frame < profile->Frames(); ++frame) {
      schedule.Wait(frame, [&] { return now; }, [&](std::uint64_t duration) { now += duration; });
      CHECK(now - start == frame * Client::kNanosecondsPerSecond / profile->RateHz(),
            "each frame begins on its absolute declared-rate deadline");
      ++samples;
      now += work;
    }
    schedule.Wait(
        profile->Frames(), [&] { return now; }, [&](std::uint64_t duration) { now += duration; });
    CHECK(now - start == Client::kNanosecondsPerSecond,
          "the last slot closes the one-second window without adding a render frame");
    CHECK(samples == static_cast<std::size_t>(rate),
          "the measurement window contains exactly the declared number of samples");
    now += Client::kNanosecondsPerSecond;
    schedule.Wait(profile->Frames(), [&] { return now; }, [&](std::uint64_t) { ++samples; });
    CHECK(samples == static_cast<std::size_t>(rate), "overload adds no compensating wait or frame");
  }
  return Report();
}
