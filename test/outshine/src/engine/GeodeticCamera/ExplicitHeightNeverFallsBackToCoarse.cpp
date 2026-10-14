#include "Check.h"
#include "GeodeticCamera.h"
#include "TerrainLoader.h"

namespace {
class CoarseGround final : public outshine::GroundQuery {
public:
  mutable int Calls = 0;

  outshine::GroundSample At(outshine::LongitudeLatitude) const override {
    ++Calls;
    return outshine::GroundSample::At(42);
  }

  outshine::GroundSample Resident(outshine::LongitudeLatitude at) const override { return At(at); }

  outshine::Ground::GroundBlock BlockAt(outshine::Ground::TileSpot) const override { return {}; }

  double PostM(double) const override { return 1; }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CoarseGround coarse;
  Scenario::View camera;
  camera.Placement = Scenario::CameraPlacement::Geodetic;
  camera.Geographic.SamplesHeight = true;
  camera.Geographic.Geodetic.HeightM = .2;
  const auto fine = ResolveGeodeticCamera(camera, {}, &coarse, GroundSample::At(80));
  CHECK(fine && fine->has_value(), "explicit source height resolves");
  if (fine && *fine) {
    CHECK_NEAR((**fine)[1], 80.2, 1e-7, "m", "camera offset follows the fine physical source");
  }
  const auto pending = ResolveGeodeticCamera(camera, {}, &coarse, GroundSample::Waiting());
  CHECK(pending && !pending->has_value(), "pending fine data cannot publish a coarse position");
  CHECK(!ResolveGeodeticCamera(camera, {}, &coarse, GroundSample::Missing()),
        "missing fine data remains a source error");
  CHECK(coarse.Calls == 0, "explicit source states never query or fall back to coarse terrain");
  camera.Geographic.SamplesHeight = false;
  const auto absolute = ResolveGeodeticCamera(camera, {}, nullptr, GroundSample::At(80));
  CHECK(absolute && absolute->has_value(), "absolute height needs no terrain");
  if (absolute && *absolute) {
    CHECK_NEAR((**absolute)[1], .2, 1e-7, "m", "absolute camera ignores sampled height");
  }
  return Report();
}
