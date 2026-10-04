#include "Atmosphere.h"
#include "Check.h"
#include <array>
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::medium_core;
  using namespace outshine::Test;
  constexpr float lift = 0.01f;
  for (const MediumLutSize size : {MediumLutSize{32, 32}, MediumLutSize{64, 16}}) {
    const auto bottom = multiScatterParams(
        kEarthAir, {.U = 0.5f / size.WidthPx, .V = 0.5f / size.HeightPx}, size, lift);
    const auto top = multiScatterParams(
        kEarthAir, {.U = 1.0f - 0.5f / size.WidthPx, .V = 1.0f - 0.5f / size.HeightPx}, size, lift);
    CHECK(bottom.CosZenith == -1.0f && top.CosZenith == 1.0f,
          "the endpoint texels represent opposite physical sun directions");
    CHECK_NEAR(bottom.RadiusKm,
               kEarthAir.BottomRadiusKm + lift,
               0.0005,
               "ground endpoint",
               "the first row starts at the declared ground lift");
    CHECK_NEAR(top.RadiusKm,
               kEarthAir.TopRadiusKm,
               0.0005,
               "atmosphere endpoint",
               "the last row reaches the declared atmosphere boundary");
    for (int y = 0; y < static_cast<int>(size.HeightPx); ++y) {
      for (int x = 0; x < static_cast<int>(size.WidthPx); ++x) {
        const MediumUv centre{.U = (x + 0.5f) / size.WidthPx, .V = (y + 0.5f) / size.HeightPx};
        const auto physical = multiScatterParams(kEarthAir, centre, size, lift);
        const double expectedRadius = kEarthAir.BottomRadiusKm + lift +
                                      static_cast<double>(y) / (size.HeightPx - 1) *
                                          (kEarthAir.TopRadiusKm - kEarthAir.BottomRadiusKm - lift);
        CHECK_NEAR(physical.RadiusKm,
                   expectedRadius,
                   0.0005,
                   "radius grid",
                   "uniform row indices span the physical radius interval");
        CHECK_NEAR(physical.CosZenith,
                   2.0 * x / (size.WidthPx - 1) - 1.0,
                   0.000001,
                   "sun grid",
                   "uniform column indices span the physical cosine interval");
        const auto recovered = multiScatterSample(kEarthAir, physical, size, lift);
        CHECK_NEAR(recovered.U,
                   centre.U,
                   0.000001,
                   "sun centre",
                   "lookup returns the physical column's texel centre");
        CHECK_NEAR(recovered.V,
                   centre.V,
                   0.00001,
                   "radius centre",
                   "lookup preserves lift and independent texture dimensions");
      }
    }
  }
  return Report();
}
