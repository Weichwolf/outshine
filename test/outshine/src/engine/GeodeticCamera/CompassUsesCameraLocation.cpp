#include "GeodeticCamera.h"
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::GeographicCameraPlacement camera;
  const auto check = [&](Vec3 forward, Vec3 up) {
    const auto result = ResolveGeodeticCameraAxes(camera, {});
    CHECK(result.has_value(), "finite compass orientation resolves");
    if (!result) { return; }
    double orthogonal = 0;
    double forwardLength = 0;
    double upLength = 0;
    for (int axis = 0; axis < 3; ++axis) {
      CHECK_NEAR(result->Forward[axis], forward[axis], 1e-12, "", "analytic compass direction");
      CHECK_NEAR(result->Up[axis], up[axis], 1e-12, "", "analytic horizon orientation");
      orthogonal += result->Forward[axis] * result->Up[axis];
      forwardLength += result->Forward[axis] * result->Forward[axis];
      upLength += result->Up[axis] * result->Up[axis];
    }
    CHECK_NEAR(orthogonal, 0, 1e-12, "", "forward and up are perpendicular");
    CHECK_NEAR(forwardLength, 1, 1e-12, "", "forward has unit length");
    CHECK_NEAR(upLength, 1, 1e-12, "", "up has unit length");
  };
  check({{0, 0, -1}}, {{0, 1, 0}});
  camera.BearingDeg = 90;
  check({{1, 0, 0}}, {{0, 1, 0}});
  camera.Geodetic.LongitudeDeg = 90;
  check({{0, -1, 0}}, {{1, 0, 0}});
  camera.Geodetic.LongitudeDeg = 180;
  check({{-1, 0, 0}}, {{0, -1, 0}});
  camera.Geodetic.LongitudeDeg = 90;
  camera.BearingDeg = 0;
  check({{0, 0, -1}}, {{1, 0, 0}});
  camera.BearingDeg = 180;
  check({{0, 0, 1}}, {{1, 0, 0}});
  camera.BearingDeg = 90;
  camera.PitchDeg = 90;
  check({{1, 0, 0}}, {{0, 1, 0}});
  camera.PitchDeg = -90;
  check({{-1, 0, 0}}, {{0, -1, 0}});
  for (double invalid :
       {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
    camera.PitchDeg = invalid;
    CHECK(!ResolveGeodeticCameraAxes(camera, {}), "invalid pitch is rejected");
    camera.PitchDeg = 0;
    camera.BearingDeg = invalid;
    CHECK(!ResolveGeodeticCameraAxes(camera, {}), "invalid bearing is rejected");
    camera.BearingDeg = 0;
  }
  return Report();
}
