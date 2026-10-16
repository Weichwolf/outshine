#include "Atmosphere.h"
#include "Check.h"
#include <cmath>

namespace {
constexpr float kHomogeneousScaleHeightKm = 1e20f;
constexpr outshine::Vec3f kExtinctionPerKm = {0.0001f, 0.0002f, 0.0003f};
constexpr outshine::MediumLutSize kSolarSize = {.WidthPx = 512, .HeightPx = 64};
constexpr double kTransmissionError = 2e-6;
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Medium medium = kEarthAir;
  medium.RayleighScaleHeightKm = kHomogeneousScaleHeightKm;
  medium.MieScaleHeightKm = kHomogeneousScaleHeightKm;
  medium.RayleighScatteringPerKm = kExtinctionPerKm;
  medium.MieScatteringPerKm = 0;
  medium.MieExtinctionPerKm = 0;
  medium.OzoneAbsorptionPerKm = {0, 0, 0};
  for (int row = 0; row < 64; ++row) {
    const auto look = medium_core::mediumTransmittanceTexel(
        medium,
        {.U = (kSolarSize.WidthPx - 0.5f) / kSolarSize.WidthPx,
         .V = (static_cast<float>(row) + 0.5f) / kSolarSize.HeightPx},
        kSolarSize);
    const double radius = look.RadiusKm;
    const double projection = radius * look.CosZenith;
    const double top = medium.TopRadiusKm;
    const double distance =
        -projection + std::sqrt(projection * projection + (top - radius) * (top + radius));
    const auto through = MediumTransmittanceToTop(medium, look, kTransmittanceSteps);
    for (int channel = 0; channel < 3; ++channel) {
      const double expected = std::exp(-medium.RayleighScatteringPerKm[channel] * distance);
      CHECK_NEAR(through[channel],
                 expected,
                 kTransmissionError,
                 "T",
                 "the tangent uses the full unoccluded column");
    }
    CHECK(MediumTransmittanceToTop(medium, look, 0) == (Vec3f{1, 1, 1}),
          "zero integration work preserves the existing identity contract");
  }
  const MediumLook down = {.RadiusKm = medium.BottomRadiusKm + 1, .CosZenith = -1};
  const auto bounded = MediumTransmittance(medium, down, kTransmittanceSteps);
  for (int channel = 0; channel < 3; ++channel) {
    CHECK_NEAR(bounded[channel],
               std::exp(-medium.RayleighScatteringPerKm[channel]),
               kTransmissionError,
               "T",
               "ordinary medium integration still stops at the solid ground");
  }
  return outshine::Test::Report();
}
