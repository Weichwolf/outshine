#include <cmath>
#include <cstdio>

#include "Check.h"

#include "ParticipatingMedium.h"

using outshine::Render::kMultiScatterGrid;
using outshine::Render::kMultiScatterLutSize;
using outshine::Render::kMultiScatterSteps;
using outshine::Render::kTransmittanceSteps;
using outshine::Render::Medium;
using outshine::Render::MediumMultiScatterTexel;
using outshine::Render::MediumTransmittance;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Medium medium;
  const auto toSun = [&](float radiusKm, float cosZenith, float out[3]) {
    MediumTransmittance(medium, radiusKm, cosZenith, kTransmittanceSteps, out);
  };

  size_t walked = 0;
  size_t finite = 0;
  float transferHigh = 0.0f;
  float luminanceHigh = 0.0f;
  double groundRow[kMultiScatterLutSize] = {};
  bool everyPsiAboveL = true;
  for (uint32_t y = 0; y < kMultiScatterLutSize; ++y) {
    for (uint32_t x = 0; x < kMultiScatterLutSize; ++x) {
      const float u = ((float)x + 0.5f) / (float)kMultiScatterLutSize;
      const float v = ((float)y + 0.5f) / (float)kMultiScatterLutSize;
      float luminance[3], transfer[3];
      MediumMultiScatterTexel(medium, u, v, toSun, luminance, transfer);
      bool sane = true;
      for (int channel = 0; channel < 3; ++channel) {
        sane = sane && std::isfinite(luminance[channel]) && std::isfinite(transfer[channel]) &&
               luminance[channel] >= 0.0f && transfer[channel] >= 0.0f &&
               transfer[channel] < 1.0f;
        transferHigh = std::fmax(transferHigh, transfer[channel]);
        luminanceHigh = std::fmax(luminanceHigh, luminance[channel]);
        if (luminance[channel] > 0.0f &&
            luminance[channel] / (1.0f - transfer[channel]) < luminance[channel]) {
          everyPsiAboveL = false;
        }
      }
      if (sane) { ++finite; }
      if (y == 0) { groundRow[x] = (double)luminance[1]; }
      ++walked;
    }
  }

  Note("texels of the 32x32 table walked", (double)walked, "texels");
  Note("directions integrated per texel", (double)(kMultiScatterGrid * kMultiScatterGrid),
       "rays");
  Note("march steps per direction", (double)kMultiScatterSteps, "samples");
  Note("the largest transfer term anywhere", transferHigh, "");
  Note("the largest second-order luminance", luminanceHigh, "per unit sun illuminance");

  CHECK(walked == (size_t)kMultiScatterLutSize * kMultiScatterLutSize && finite == walked,
        "**EVERY TEXEL OF THE MULTIPLE SCATTERING TABLE IS FINITE, NON-NEGATIVE, AND ITS TRANSFER "
        "TERM IS BELOW ONE.** The transfer f_ms is the fraction of once-scattered light that "
        "scatters again; Hillaire's closed form 1/(1-f_ms) sums every order as a geometric series, "
        "which CONVERGES only while f_ms < 1 -- so the bound is not hygiene, it is the series' own "
        "admission condition, checked over the whole domain");
  CHECK(transferHigh < 0.5f,
        "and for Earth's coefficients the transfer stays under a half -- the series' ratio is "
        "small, which is why five unrolled orders and the closed form agree in the reference");
  CHECK(everyPsiAboveL,
        "the summed orders are never less than the second order alone, which is what adding "
        "non-negative orders means");

  {
    const double night = groundRow[0];
    const double noon = groundRow[kMultiScatterLutSize - 1];
    Note("green second order at the ground, sun straight down", night, "");
    Note("green second order at the ground, sun at the zenith", noon, "");
    CHECK(noon > 0.0 && night < noon * 1.0e-2,
          "**A SUN BELOW THE PLANET LIGHTS NOTHING**: at the table's left edge the sun is straight "
          "down, every path to it passes through the planet, and the second order collapses to "
          "under a hundredth of noon's -- the residual arriving through the twilight ring the "
          "planet's own shadow leaves near the horizon");
    bool rises = true;
    for (uint32_t x = kMultiScatterLutSize / 2; x + 1 < kMultiScatterLutSize; ++x) {
      if (groundRow[x + 1] < groundRow[x] * 0.999) { rises = false; }
    }
    CHECK(rises,
          "and from the horizon to the zenith the second order only grows -- a higher sun lights "
          "more of the sphere of directions, texel by texel across the daylight half of the row");
  }

  Covers("I.18.3 the medium's multiple scattering follows Hillaire's isotropic transfer: a 32x32 "
         "table whose every texel converges, bounded by the physics that a sun below the planet "
         "lights nothing and a higher sun lights more");
  return Report();
}
