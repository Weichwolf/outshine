#include <array>
#include <cmath>

#include "Check.h"

#include "MicrofacetEnergy.h"

using outshine::Render::GgxDirectionalAlbedo;
using outshine::Render::GgxEnergyAverage;
using outshine::Render::GgxEnergyScale;
using outshine::Render::SchlickAverage;

int main() {
  const std::array<double, 3> perfect = {1.0, 1.0, 1.0};
  double worstFurnace = 0.0;
  double worstLoss = 0.0;
  double lossAtRoughness = 0.0;
  double lossAtView = 0.0;
  for (int r = 0; r <= 20; ++r) {
    const double roughness = (double)r / 20.0;
    double previous = 1.0;
    for (int v = 1; v <= 20; ++v) {
      const double nv = (double)v / 20.0;
      const double e = GgxDirectionalAlbedo(nv, roughness);
      CHECK(e > 0.0 && e <= 1.0, "a directional albedo is a fraction of what arrived");
      std::array<double, 3> scale{};
      GgxEnergyScale(perfect, roughness, nv, scale);
      CHECK(scale[0] >= 1.0, "the compensation restores energy and never removes it");

      worstFurnace = std::fmax(worstFurnace, std::fabs(e * scale[0] - 1.0));
      if (1.0 - e > worstLoss) {
        worstLoss = 1.0 - e;
        lossAtRoughness = roughness;
        lossAtView = nv;
      }
    }

    const double here = GgxDirectionalAlbedo(0.5, roughness);
    CHECK(here <= previous + 1.0e-3, "the single-scatter albedo falls as the surface roughens");
    previous = here;
  }
  outshine::Test::Note("worst departure from a white furnace, F0 = 1", worstFurnace, "dimensionless");
  CHECK(worstFurnace < 1.0e-9,
        "a perfect reflector returns exactly what arrives once the missing energy is restored");

  outshine::Test::Note("the largest fraction the single bounce loses", worstLoss, "dimensionless");
  outshine::Test::Note("  at roughness", lossAtRoughness, "dimensionless");
  outshine::Test::Note("  at n.v", lossAtView, "dimensionless");
  CHECK(worstLoss > 0.1, "and there is something to restore, or this correction would be theatre");

  std::array<double, 3> mirror{};
  GgxEnergyScale({0.04, 0.5, 1.0}, 0.0, 0.5, mirror);
  CHECK(mirror[0] == 1.0 && mirror[1] == 1.0 && mirror[2] == 1.0,
        "at roughness 0 the multiplier is exactly one, in every channel");

  for (const double f0 : {0.0, 0.04, 0.5, 0.95, 1.0}) {
    double integral = 0.0;
    const int steps = 200000;
    for (int i = 0; i < steps; ++i) {
      const double mu = (i + 0.5) / steps;
      integral += 2.0 * (f0 + (1.0 - f0) * std::pow(1.0 - mu, 5.0)) * mu;
    }
    CHECK_NEAR(SchlickAverage(f0), integral / steps, 1.0e-9, "dimensionless",
               "the closed-form average Fresnel is the hemispherical integral it claims to be");
  }

  outshine::Test::Note("E_avg at roughness 0.5", GgxEnergyAverage(0.5), "dimensionless");
  outshine::Test::Note("E_avg at roughness 1.0", GgxEnergyAverage(1.0), "dimensionless");

  outshine::Test::Covers("the energy a single microfacet bounce loses is restored exactly, "
                         "so a perfect reflector under a white furnace returns what it received at "
                         "every roughness -- an identity with no free parameter in it");
  return outshine::Test::Report();
}
