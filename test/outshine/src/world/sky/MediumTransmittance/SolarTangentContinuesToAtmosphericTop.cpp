#include "Atmosphere.h"
#include "Check.h"
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Medium medium = kEarthAir;
  medium.RayleighScaleHeightKm = 1e20f;
  medium.MieScaleHeightKm = 1e20f;
  medium.RayleighScatteringPerKm = {0.0001f, 0.0002f, 0.0003f};
  medium.MieScatteringPerKm = 0;
  medium.MieExtinctionPerKm = 0;
  medium.OzoneAbsorptionPerKm = {0, 0, 0};
  for (int row = 0; row < 64; ++row) {
    const auto look = medium_core::mediumTransmittanceTexel(
        medium, {.U = 511.5f / 512, .V = (static_cast<float>(row) + 0.5f) / 64}, {512, 64});
    const double radius = look.RadiusKm;
    const double projection = radius * look.CosZenith;
    const double top = medium.TopRadiusKm;
    const double distance =
        -projection + std::sqrt(projection * projection + (top - radius) * (top + radius));
    const auto through = MediumTransmittanceToTop(medium, look, kTransmittanceSteps);
    for (int channel = 0; channel < 3; ++channel) {
      const double expected = std::exp(-medium.RayleighScatteringPerKm[channel] * distance);
      CHECK_NEAR(
          through[channel], expected, 2e-6, "T", "the tangent uses the full unoccluded column");
    }
    CHECK(MediumTransmittanceToTop(medium, look, 0) == (Vec3f{1, 1, 1}),
          "zero integration work preserves the existing identity contract");
  }
  const MediumLook down = {.RadiusKm = medium.BottomRadiusKm + 1, .CosZenith = -1};
  const auto bounded = MediumTransmittance(medium, down, kTransmittanceSteps);
  for (int channel = 0; channel < 3; ++channel) {
    CHECK_NEAR(bounded[channel],
               std::exp(-medium.RayleighScatteringPerKm[channel]),
               2e-6,
               "T",
               "ordinary medium integration still stops at the solid ground");
  }
  return outshine::Test::Report();
}
