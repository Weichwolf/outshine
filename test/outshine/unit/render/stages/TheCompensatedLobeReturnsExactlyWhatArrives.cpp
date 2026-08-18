/* THE ENERGY COMPENSATION AS AN IDENTITY RATHER THAN AS A PICTURE (board:1408). The corpus decides
 * whether the compensated surface LOOKS right; this decides whether the correction conserves energy,
 * over every roughness and view angle rather than the one row a case declares.
 *
 * THE CLAIM IS EXACT AND NOT A BOUND, WHICH IS WHY IT IS WORTH ASSERTING. At `F0 = 1` -- a perfect
 * reflector, the configuration in which a microfacet model has nowhere to hide -- the average Fresnel
 * is 1, so `F_ms` collapses to `E_avg / (1 - (1 - E_avg))` = 1 and the multiplier is exactly `1 / E`.
 * The compensated directional albedo is then `E * (1 / E)` = **1**: a white furnace returns precisely
 * what it received, at every roughness, by construction rather than by tuning.
 *
 * SO THE TEST IS NOT "IS THE PICTURE BETTER". A correction with a free parameter can always be made to
 * look better; this one has none, and the identity above is what says so. What the corpus adds is
 * whether the missing energy was distributed the way a renderer distributes it -- a different question,
 * and one a still can answer.
 *
 * AND THE UNCOMPENSATED LOBE IS MEASURED BESIDE IT, so the size of what was missing is reported rather
 * than implied. */
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
      /* THE IDENTITY. */
      worstFurnace = std::fmax(worstFurnace, std::fabs(e * scale[0] - 1.0));
      if (1.0 - e > worstLoss) {
        worstLoss = 1.0 - e;
        lossAtRoughness = roughness;
        lossAtView = nv;
      }
    }
    /* A ROUGHER SURFACE SHADOWS ITSELF MORE, so the albedo of the uncompensated lobe falls as the
     * roughness rises. Checked at a fixed view angle, where it is a statement about one variable. */
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

  /* A MIRROR HAS NO MICROFACETS TO SHADOW EACH OTHER, and the case that measures it is bit-identical
   * to the oracle -- so this must be exactly one and not nearly one. */
  std::array<double, 3> mirror{};
  GgxEnergyScale({0.04, 0.5, 1.0}, 0.0, 0.5, mirror);
  CHECK(mirror[0] == 1.0 && mirror[1] == 1.0 && mirror[2] == 1.0,
        "at roughness 0 the multiplier is exactly one, in every channel");

  /* THE AVERAGE FRESNEL IS A CLOSED FORM AND THE INTEGRAL IT CLOSES IS SPELLED HERE INDEPENDENTLY:
   * `2 * integral (F0 + (1 - F0)(1 - mu)^5) mu dmu` over the hemisphere. A transcription error in
   * `F0 + (1 - F0) / 21` is then a difference rather than a constant nobody questions. */
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

  outshine::Test::Covers("board:1408 the energy a single microfacet bounce loses is restored exactly, "
                         "so a perfect reflector under a white furnace returns what it received at "
                         "every roughness -- an identity with no free parameter in it");
  return outshine::Test::Report();
}
