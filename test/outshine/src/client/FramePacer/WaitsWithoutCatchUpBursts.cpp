#include "src/client/FramePacer.h"
#include "Check.h"
#include <cstdint>

int main() {
  using namespace outshine::Test;
  outshine::Client::FramePacer pacer(100);
  std::uint64_t now = 0;
  std::uint64_t waited = 0;
  unsigned sleeps = 0;
  const auto clock = [&] { return now; };
  const auto sleep = [&](std::uint64_t duration) {
    waited = duration;
    now += duration;
    ++sleeps;
  };
  pacer.Wait(clock, sleep);
  CHECK(sleeps == 0, "first frame starts immediately, including clock origin zero");
  now = 30;
  pacer.Wait(clock, sleep);
  CHECK(sleeps == 1 && waited == 70 && now == 100, "only unused frame time sleeps");
  now = 450;
  pacer.Wait(clock, sleep);
  CHECK(sleeps == 1, "slow frame receives no additional delay");
  now = 460;
  pacer.Wait(clock, sleep);
  CHECK(sleeps == 2 && waited == 90 && now == 550, "slow frame creates no catch-up burst");
  now = 570;
  pacer.Wait(clock, [&](std::uint64_t duration) { now += duration + 40; });
  CHECK(now == 690, "scheduler oversleep is accepted without spinning");
  now = 700;
  pacer.Wait(clock, sleep);
  CHECK(waited == 90 && now == 790, "next deadline follows actual wake time");
  now = 890;
  pacer.Wait(clock, sleep);
  CHECK(sleeps == 3, "exact deadline needs no sleep");
  return Report();
}
