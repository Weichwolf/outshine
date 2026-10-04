#include "Atmosphere.h"
#include "Check.h"
#include <array>
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::medium_core;
  using namespace outshine::Test;
  for (const float resolution : {2.0f, 32.0f, 64.0f, 192.0f, 512.0f}) {
    for (const float unit : {0.0f, 0.1f, 0.5f, 0.9f, 1.0f}) {
      const double expected = (0.5 + unit * (resolution - 1.0)) / resolution;
      CHECK_NEAR(unitToSubUvs(unit, resolution),
                 expected,
                 0.0000001,
                 "texel centres",
                 "the endpoints are half a texel inside the table");
      CHECK_NEAR(subUvsToUnit(unitToSubUvs(unit, resolution), resolution),
                 unit,
                 0.000001,
                 "coordinate inverse",
                 "table encoding and decoding use the same finite extent");
    }
  }
  const MediumLutSize size{.WidthPx = 192.0f, .HeightPx = 108.0f};
  const float ground = kEarthAir.BottomRadiusKm;
  const auto zenith = skyViewSample(ground, ground, {.CosView = 1.0f, .LightViewCos = 1.0f}, size);
  const auto nadir = skyViewSample(ground, ground, {.CosView = -1.0f, .LightViewCos = -1.0f}, size);
  CHECK_NEAR(zenith.Uv.U,
             0.5 / size.WidthPx,
             0.000001,
             "sun azimuth",
             "the sunward endpoint uses the first column centre");
  CHECK_NEAR(zenith.Uv.V,
             0.5 / size.HeightPx,
             0.000001,
             "zenith",
             "the upper pole uses the first row centre");
  CHECK_NEAR(nadir.Uv.U,
             1.0 - 0.5 / size.WidthPx,
             0.000001,
             "opposite azimuth",
             "the opposite endpoint uses the last column centre");
  CHECK_NEAR(nadir.Uv.V,
             1.0 - 0.5 / size.HeightPx,
             0.000001,
             "nadir",
             "the lower pole uses the last row centre");
  CHECK(!zenith.HitsGround && nadir.HitsGround,
        "opposite poles agree with analytic spherical visibility");
  for (const float heightKm : {0.0f, 0.002f, 2.0f, 100.0f, 640.0f}) {
    for (const float u : {0.05f, 0.3f, 0.7f, 0.95f}) {
      for (const float v : {0.05f, 0.2f, 0.4f, 0.6f, 0.8f, 0.95f}) {
        const MediumUv encoded{.U = unitToSubUvs(u, size.WidthPx),
                               .V = unitToSubUvs(v, size.HeightPx)};
        const auto look = skyViewParams(kEarthAir, ground + heightKm, encoded, size);
        const auto decoded = skyViewSample(ground, ground + heightKm, look, size);
        CHECK_NEAR(decoded.Uv.U,
                   encoded.U,
                   0.000002,
                   "azimuth inverse",
                   "the table lookup recovers its declared horizontal angle");
        CHECK_NEAR(decoded.Uv.V,
                   encoded.V,
                   0.0001,
                   "zenith inverse",
                   "lookup and generation share the horizon at ground, flight and orbit heights");
        CHECK(decoded.HitsGround == (v > 0.5f),
              "the two table halves retain sky and ground classification");
      }
    }
  }
  return Report();
}
