#include "Atmosphere.h"
#include "Check.h"
#include <cmath>
#include <algorithm>

namespace {
constexpr double kTransmittanceTolerance = 0.002;

double OzoneIntegral(const outshine::Medium &medium, double heightKm) {
  const double width = medium.OzoneHalfWidthKm;
  const double aboveStart = std::clamp(heightKm - (medium.OzoneCentreKm - width), 0.0, 2.0 * width);
  if (aboveStart <= width) { return aboveStart * aboveStart / (2.0 * width); }
  const double belowEnd = 2.0 * width - aboveStart;
  return width - belowEnd * belowEnd / (2.0 * width);
}

double VerticalThrough(const outshine::Medium &medium, double fromKm, double toKm, int channel) {
  const auto column = [fromKm, toKm](double scaleKm) {
    return scaleKm * (std::exp(-fromKm / scaleKm) - std::exp(-toKm / scaleKm));
  };
  const double rayleigh =
      medium.RayleighScatteringPerKm[channel] * column(medium.RayleighScaleHeightKm);
  const double mie = medium.MieExtinctionPerKm * column(medium.MieScaleHeightKm);
  const double ozone = medium.OzoneAbsorptionPerKm[channel] *
                       (OzoneIntegral(medium, toKm) - OzoneIntegral(medium, fromKm));
  return std::exp(-rayleigh - mie - ozone);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const double haze : {0.0, 0.25, 1.0, 4.0}) {
    const Medium medium = Hazed(kEarthAir, haze);
    for (const float heightKm : {0.005f, 0.05f, 0.5f, 1.0f, 5.0f, 10.0f, 25.0f, 50.0f, 99.0f}) {
      const MediumLook look{.RadiusKm = medium.BottomRadiusKm + heightKm, .CosZenith = 1.0f};
      const Vec3f actual = MediumTransmittance(medium, look, kTransmittanceSteps);
      const double startKm = static_cast<double>(look.RadiusKm) - medium.BottomRadiusKm;
      const double endKm = static_cast<double>(medium.TopRadiusKm) - medium.BottomRadiusKm;
      for (int channel = 0; channel < 3; ++channel) {
        CHECK_NEAR(actual[channel],
                   VerticalThrough(medium, startKm, endKm, channel),
                   kTransmittanceTolerance,
                   "solar transmission",
                   "native budget resolves the analytic vertical optical column");
      }
      CHECK(MediumTransmittance(medium, look, 0) == (Vec3f{1.0f, 1.0f, 1.0f}),
            "zero samples retains the existing identity transfer");
      for (const int steps : {1, 2, 3, 39, 41}) {
        const Vec3f finite = MediumTransmittance(medium, look, steps);
        for (int channel = 0; channel < 3; ++channel) {
          CHECK(std::isfinite(finite[channel]) && finite[channel] >= 0.0f &&
                    finite[channel] <= 1.0f,
                "small and odd sample budgets retain a finite positive transfer");
        }
      }
    }
  }
  return Report();
}
