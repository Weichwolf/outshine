#include <Outshine.h>
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  engine.keepSamples(8);
  for (const double elapsed : {-1.0,
                               std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()}) {
    const auto result = engine.advance(elapsed);
    CHECK(!result && result.error().contains("finite nonnegative"),
          "invalid time is rejected by its own input contract");
    CHECK(engine.advance(0.0).has_value(), "invalid time leaves no queued simulation work");
    CHECK(!engine.standing(), "invalid time does not create a render scene");
  }
  std::vector<double> samples;
  engine.stepTimesMs(samples);
  CHECK(samples.empty(), "invalid times execute no recorded steps");
  return Report();
}
