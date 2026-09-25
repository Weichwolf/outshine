#include <Outshine.h>
#include "Check.h"
#include <chrono>
#include <limits>
#include <thread>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  size_t callbacks = 0;
  for (double seconds : {-1.0,
                         std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()}) {
    const auto result = engine.preload(seconds, [&](const Loading &) { ++callbacks; });
    CHECK(!result, "invalid deadline is rejected even when there is no ground to load");
    CHECK(callbacks == 0, "invalid deadline performs no progress callback");
    CHECK(!engine.preload(seconds), "both overloads enforce the same deadline contract");
    CHECK(!engine.preload(seconds, WorldQuality::Refined),
          "quality-directed preload rejects invalid deadlines before work");
  }
  CHECK(engine.loading().PreloadMs == 0.0, "invalid calls do not become preload samples");
  CHECK(
      engine
          .preload(
              0, [](const Loading &) { std::this_thread::sleep_for(std::chrono::milliseconds(2)); })
          .has_value(),
      "zero budget permits an already complete empty world");
  CHECK(engine.loading().PreloadMs >= 1.0,
        "loading snapshot records the completed preload call, including its callback");
  CHECK(engine.preload(0, WorldQuality::Refined).has_value(),
        "a groundless scene satisfies quality-directed preload without waiting");
  return Report();
}
