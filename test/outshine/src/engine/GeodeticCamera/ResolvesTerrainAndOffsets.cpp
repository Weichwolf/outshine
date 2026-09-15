#include "GeodeticCamera.h"
#include "TerrainLoader.h"
#include "math/Units.h"
#include "Check.h"
#include <limits>

namespace {
class Terrain final : public outshine::GroundQuery {
public:
  outshine::GroundSample Sample = outshine::GroundSample::Waiting();
  mutable outshine::LongitudeLatitude Requested;

  outshine::GroundSample At(outshine::LongitudeLatitude at) const override {
    Requested = at;
    return Sample;
  }

  outshine::GroundSample Resident(outshine::LongitudeLatitude at) const override { return At(at); }

  outshine::Ground::GroundBlock BlockAt(outshine::Ground::TileSpot) const override { return {}; }

  double PostM(double) const override { return 1; }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::View camera;
  camera.Placement = Scenario::CameraPlacement::Geodetic;
  camera.Geographic.Geodetic = {.LongitudeDeg = 90, .LatitudeDeg = 0, .HeightM = 12};
  camera.OffsetM = {{3, 4, 5}};
  const auto check = [&](const auto &result, double east, double up, double south) {
    CHECK(result && result->has_value(), "resolved position exists");
    if (result && *result) {
      CHECK_NEAR((**result)[0], east, 1e-7, "m", "east follows the ellipsoid and offset");
      CHECK_NEAR((**result)[1], up, 1e-7, "m", "up includes Earth curvature and offset");
      CHECK_NEAR((**result)[2], south, 1e-7, "m", "native south axis and offset");
    }
  };
  // At equatorial longitudes 0 and 90, ECEF axes give an independent closed-form oracle.
  check(ResolveGeodeticCamera(camera, {}, nullptr), kWgs84A + 15, -kWgs84A + 4, 5);
  camera.Geographic.SamplesHeight = true;
  CHECK(!ResolveGeodeticCamera(camera, {}, nullptr),
        "sampling without declared ground is an error");
  Terrain terrain;
  const auto pending = ResolveGeodeticCamera(camera, {}, &terrain);
  CHECK(pending && !*pending, "pending terrain publishes no invented position");
  CHECK(terrain.Requested.LongitudeDeg == 90 && terrain.Requested.LatitudeDeg == 0,
        "terrain request follows the camera rather than the world origin");
  terrain.Sample = GroundSample::Missing();
  CHECK(!ResolveGeodeticCamera(camera, {}, &terrain), "a terrain hole remains an error");
  terrain.Sample = GroundSample::At(42);
  check(ResolveGeodeticCamera(camera, {}, &terrain), kWgs84A + 57, -kWgs84A + 4, 5);
  camera.Geographic.Geodetic.LongitudeDeg = 0;
  check(ResolveGeodeticCamera(camera, {}, &terrain), 3, 58, 5);
  camera.OffsetM[0] = std::numeric_limits<double>::infinity();
  CHECK(!ResolveGeodeticCamera(camera, {}, &terrain), "infinite offsets are rejected");
  camera.OffsetM[0] = 0;
  camera.Geographic.Geodetic.LatitudeDeg = 91;
  CHECK(!ResolveGeodeticCamera(camera, {}, &terrain), "invalid geographic latitude is rejected");
  return Report();
}
