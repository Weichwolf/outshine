#include "SunShadowPass.h"
#include "Check.h"

#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Render::SunShadowPass shadow;
  const Render::SunShadowPass::Overhead sun{.ToSun = {{0, 1, 0}}, .Up = {{0, 1, 0}}};
  CHECK(!shadow.IsDeclared() && shadow.RegionCount() == 0, "an undeclared atlas has no regions");
  shadow.Declare(sun, 10);
  CHECK(shadow.IsDeclared() && shadow.RegionCount() == 1, "a valid bounded shadow has one region");
  shadow.Declare(sun, 10, true);
  CHECK(shadow.IsDeclared() && shadow.RegionCount() == Render::kSunShadowRegions,
        "a valid camera-centred shadow has its cascades");
  for (const double radius : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN()}) {
    shadow.Declare(sun, radius, true);
    CHECK(!shadow.IsDeclared() && shadow.RegionCount() == 0,
          "invalid or disabled shadow bounds expose no stale regions");
  }
  shadow.Declare({.ToSun = {{0, 0, 0}}, .Up = {{0, 1, 0}}}, 10);
  CHECK(!shadow.IsDeclared() && shadow.RegionCount() == 0, "a missing light basis has no regions");
  return Report();
}
