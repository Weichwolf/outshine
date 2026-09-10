#include <Outshine.h>
#include "Check.h"
#include <limits>

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
  }
  CHECK(engine.preload(0).has_value(), "zero budget permits an already complete empty world");
  return Report();
}
