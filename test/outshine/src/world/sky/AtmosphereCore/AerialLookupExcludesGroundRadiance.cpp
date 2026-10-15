#include "Atmosphere.h"
#include "Check.h"
#include <algorithm>
#include <cmath>

namespace {
void CheckSample(float heightKm, outshine::MediumLutSize size, outshine::SkyViewLook look) {
  using namespace outshine;
  using namespace outshine::medium_core;
  using namespace outshine::Test;
  const float ground = kEarthAir.BottomRadiusKm;
  const float radius = ground + heightKm;
  const auto uv = skyViewAtmosphereSample(ground, radius, look, size);
  CHECK(std::isfinite(uv.U) && std::isfinite(uv.V), "air lookup always has finite coordinates");
  const double expectedU =
      (0.5 + std::sqrt((1.0 - look.LightViewCos) * 0.5) * (size.WidthPx - 1.0)) / size.WidthPx;
  CHECK_NEAR(uv.U, expectedU, 1e-7, "texture U", "air preserves the declared relative sun azimuth");
  CHECK(uv.V >= 0.5f / size.HeightPx, "the air lookup stays inside the first row centre");
  const int skyRows = static_cast<int>(size.HeightPx) / 2;
  const double row = static_cast<double>(uv.V) * size.HeightPx - 0.5;
  const int low = static_cast<int>(std::floor(row));
  const double blend = row - low;
  const double groundShare =
      (low >= skyRows ? 1.0 - blend : 0.0) + (low + 1 >= skyRows ? blend : 0.0);
  CHECK_NEAR(groundShare,
             0.0,
             5e-6,
             "bilinear weight",
             "ground radiance cannot contaminate aerial in-scattering at or below the horizon");
  if (look.CosView == 1.0f) {
    CHECK_NEAR(uv.V,
               0.5 / size.HeightPx,
               1e-7,
               "texture V",
               "the zenith uses the first row centre rather than the old extent-plus-one mapping");
  }
  if (look.CosView == -1.0f) {
    CHECK_NEAR(uv.V,
               (skyRows - 0.5) / size.HeightPx,
               1e-7,
               "texture V",
               "downward geometry uses the last sky row instead of the virtual ground");
  }
  if (look.CosView == 0.5f) {
    const double radiusD = radius;
    const double horizon = std::acos(-std::sqrt(radiusD * radiusD - ground * ground) / radiusD);
    const double unit = 0.5 * (1.0 - std::sqrt(1.0 - std::acos(0.5) / horizon));
    const double expectedV = std::min((0.5 + unit * (size.HeightPx - 1.0)) / size.HeightPx,
                                      (skyRows - 0.5) / size.HeightPx);
    CHECK_NEAR(uv.V,
               expectedV,
               3e-5,
               "texture V",
               "an interior sky direction keeps the nonlinear spherical angle mapping");
  }
}
}

int main() {
  for (const float heightKm : {0.0f, 0.002f, 2.0f, 100.0f, 640.0f}) {
    for (const float rows : {2.0f, 32.0f, 108.0f, 109.0f}) {
      for (const float cosView : {1.0f, 0.5f, 0.0f, -0.5f, -1.0f}) {
        for (const float azimuth : {1.0f, 0.0f, -1.0f}) {
          CheckSample(heightKm, {.WidthPx = 192.0f, .HeightPx = rows}, {cosView, azimuth});
        }
      }
    }
  }
  return outshine::Test::Report();
}
