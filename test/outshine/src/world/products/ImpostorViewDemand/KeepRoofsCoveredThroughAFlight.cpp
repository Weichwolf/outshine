#include "Check.h"
#include "ImpostorViewDemand.h"
#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  const std::array directions{
      Vec3{{0.8, 0.6, 0}}, Vec3{{-0.8, 0.6, 0}}, Vec3{{0.28, 0, 0.96}}, Vec3{{-0.28, 0, 0.96}}};
  const Vec3 east{{3, 1, 2}};
  const Vec3 west{{-3, 1, 2}};

  CHECK(!SelectImpostorDirections(directions, {east, east}, true).empty() &&
            !SelectImpostorDirections(directions, {west, west}, true).empty(),
        "both endpoints have a view that sees the roofs");
  CHECK(SelectImpostorDirections(directions, {east, west}, true).empty(),
        "keep geometry when the best two views lose the roofs between endpoints");
  CHECK(!SelectImpostorDirections(directions, {east, west}).empty(),
        "other products can still use the same directions");

  const auto smaller = ImpostorProjectionRamp(200, 128, 700, 2);
  const auto larger = ImpostorProjectionRamp(200, 256, 700);
  CHECK(smaller.BeginM == larger.BeginM && smaller.EndM == larger.EndM,
        "an authored two-pixel limit moves the band to the equivalent angular footprint");
  return Report();
}
