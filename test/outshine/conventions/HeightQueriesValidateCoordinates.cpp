#include <Outshine.h>
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  constexpr double nan = std::numeric_limits<double>::quiet_NaN();
  constexpr double inf = std::numeric_limits<double>::infinity();
  for (const double value : {nan, inf, -inf, 181.0, -181.0}) {
    const auto result = engine.sampleHeight({.LongitudeDeg = value});
    CHECK(!result && result.error().contains("requires finite longitude"),
          "invalid longitude is rejected before world access");
  }
  for (const double value : {nan, inf, -inf, 91.0, -91.0}) {
    const auto result = engine.sampleHeight({.LatitudeDeg = value});
    CHECK(!result && result.error().contains("requires finite longitude"),
          "invalid latitude is rejected before world access");
  }
  for (const double latitude : {-90.0, -86.0, 86.0, 90.0}) {
    const auto result = engine.sampleHeight({.LatitudeDeg = latitude});
    CHECK(!result && result.error().contains("outside Mercator"),
          "polar queries do not sample a clamped replacement location");
  }
  for (const double longitude : {-180.0, 0.0, 180.0}) {
    const auto result = engine.sampleHeight({.LongitudeDeg = longitude, .HeightM = nan});
    CHECK(!result && result.error().contains("no world"),
          "valid longitude boundaries and ignored height reach world validation");
  }
  return Report();
}
