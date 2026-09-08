#include <array>
#include <numbers>
#include "AzimuthElevation.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<Vec3, 4> cardinal = {{{{0, 0, -1}}, {{1, 0, 0}}, {{0, 0, 1}}, {{-1, 0, 0}}}};
  for (size_t at = 0; at < cardinal.size(); ++at) {
    const Vec3 direction = EastUpSouthDirection(at * std::numbers::pi / 2, 0);
    for (size_t axis = 0; axis < 3; ++axis) {
      CHECK_NEAR(direction[axis],
                 cardinal[at][axis],
                 1e-12,
                 "unit",
                 "north/east/south/west bearings map to -Z/+X/+Z/-X in the render frame");
    }
  }
  const Vec3 zenith = EastUpSouthDirection(0, std::numbers::pi / 2);
  CHECK_NEAR(zenith[1], 1.0, 1e-12, "unit", "positive elevation points up");
  return Report();
}
