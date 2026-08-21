#include <cmath>
#include <cstdio>
#include <vector>

#include "Check.h"

#include "ParticipatingMedium.h"

using outshine::Render::kMediumGroundLiftKm;
using outshine::Render::kMultiScatterLutSize;
using outshine::Render::kSkyViewLutHeight;
using outshine::Render::kSkyViewLutWidth;
using outshine::Render::kTransmittanceSteps;
using outshine::Render::Medium;
using outshine::Render::MediumGroundReach;
using outshine::Render::MediumMultiScatterTexel;
using outshine::Render::MediumSkyRay;
using outshine::Render::MediumTransmittance;
using outshine::Render::SkyViewParams;
using outshine::Render::SkyViewUv;
using outshine::Render::SubUvsToUnit;
using outshine::Render::UnitToSubUvs;

namespace {

struct Psi {
  std::vector<float> Texels;

  void Build(const Medium &medium) {
    const auto toSun = [&](float radiusKm, float cosZenith, float out[3]) {
      MediumTransmittance(medium, radiusKm, cosZenith, kTransmittanceSteps, out);
    };
    Texels.resize((size_t)kMultiScatterLutSize * kMultiScatterLutSize * 3u);
    for (uint32_t y = 0; y < kMultiScatterLutSize; ++y) {
      for (uint32_t x = 0; x < kMultiScatterLutSize; ++x) {
        float luminance[3], transfer[3];
        MediumMultiScatterTexel(medium, ((float)x + 0.5f) / (float)kMultiScatterLutSize,
                                ((float)y + 0.5f) / (float)kMultiScatterLutSize, toSun, luminance,
                                transfer);
        for (int channel = 0; channel < 3; ++channel) {
          Texels[((size_t)y * kMultiScatterLutSize + x) * 3u + (size_t)channel] =
              luminance[channel] / (1.0f - transfer[channel]);
        }
      }
    }
  }

  void Sample(const Medium &medium, float radiusKm, float cosZenith, float out[3]) const {
    float u = cosZenith * 0.5f + 0.5f;
    float v = (radiusKm - medium.BottomRadiusKm) / (medium.TopRadiusKm - medium.BottomRadiusKm);
    u = UnitToSubUvs(u, (float)kMultiScatterLutSize);
    v = UnitToSubUvs(v, (float)kMultiScatterLutSize);
    const float x = std::fmin(std::fmax(u * (float)kMultiScatterLutSize - 0.5f, 0.0f),
                              (float)kMultiScatterLutSize - 1.0f);
    const float y = std::fmin(std::fmax(v * (float)kMultiScatterLutSize - 0.5f, 0.0f),
                              (float)kMultiScatterLutSize - 1.0f);
    const size_t x0 = (size_t)x, y0 = (size_t)y;
    const size_t x1 = x0 + 1 < kMultiScatterLutSize ? x0 + 1 : x0;
    const size_t y1 = y0 + 1 < kMultiScatterLutSize ? y0 + 1 : y0;
    const float fx = x - (float)x0, fy = y - (float)y0;
    for (int channel = 0; channel < 3; ++channel) {
      const auto at = [&](size_t tx, size_t ty) {
        return Texels[(ty * kMultiScatterLutSize + tx) * 3u + (size_t)channel];
      };
      out[channel] = (1.0f - fy) * ((1.0f - fx) * at(x0, y0) + fx * at(x1, y0)) +
                     fy * ((1.0f - fx) * at(x0, y1) + fx * at(x1, y1));
    }
  }
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Medium medium;
  const float eyeKm = medium.BottomRadiusKm + kMediumGroundLiftKm;

  Psi psi;
  psi.Build(medium);
  const auto toSun = [&](float radiusKm, float cosZenith, float out[3]) {
    MediumTransmittance(medium, radiusKm, cosZenith, kTransmittanceSteps, out);
  };
  const auto scattered = [&](float radiusKm, float cosZenith, float out[3]) {
    psi.Sample(medium, radiusKm, cosZenith, out);
  };
  const auto sky = [&](float cosView, float lightViewCos, float cosSun, float out[3]) {
    MediumSkyRay(medium, eyeKm, cosView, lightViewCos, cosSun, toSun, scattered, out);
  };

  {
    size_t walked = 0;
    double worst = 0.0;
    for (uint32_t y = 0; y < kSkyViewLutHeight; ++y) {
      for (uint32_t x = 0; x < kSkyViewLutWidth; ++x) {
        const float u = ((float)x + 0.5f) / (float)kSkyViewLutWidth;
        const float v = ((float)y + 0.5f) / (float)kSkyViewLutHeight;
        float cosView = 0.0f, lightViewCos = 0.0f;
        SkyViewParams(medium, eyeKm, u, v, &cosView, &lightViewCos);
        const bool hitsGround = MediumGroundReach(medium, eyeKm, cosView) >= 0.0f;
        float backU = 0.0f, backV = 0.0f;
        SkyViewUv(medium, eyeKm, hitsGround, cosView, lightViewCos, &backU, &backV);
        worst = std::fmax(worst, std::fmax(std::fabs((double)backU - u),
                                           std::fabs((double)backV - v)));
        ++walked;
      }
    }
    Note("texels of the 192x108 sky view walked", (double)walked, "texels");
    Note("the worst round trip drift", worst, "uv");
    Note("one texel of v", 1.0 / (double)kSkyViewLutHeight, "uv");
    CHECK(walked == (size_t)kSkyViewLutWidth * kSkyViewLutHeight && worst < 0.2 / (double)kSkyViewLutHeight,
          "**THE SKY VIEW PARAMETERISATION ROUND-TRIPS, texel for texel, to a fifth of a texel.** "
          "Hillaire bends v so half the table is sky and half is ground WHATEVER the eye height -- "
          "the horizon always falls on the middle row, which is where the gradient is steepest. A "
          "forward map that did not invert the inverse would shear the horizon");
  }

  const float noon = 1.0f;
  const float evening = std::cos(88.0f * 3.14159265358979f / 180.0f);
  {
    float up[3], side[3];
    sky(std::cos(30.0f * 3.14159265358979f / 180.0f), 1.0f, noon, up);
    sky(std::cos(85.0f * 3.14159265358979f / 180.0f), 1.0f, noon, side);
    Note("30 deg from zenith at noon: red", up[0], "");
    Note("30 deg from zenith at noon: blue", up[2], "");
    Note("5 deg above the horizon at noon: blue", side[2], "");
    CHECK(up[2] > up[1] && up[1] > up[0],
          "**THE NOON SKY IS BLUE, and it is blue by DERIVATION**: looking 30 deg off zenith, the "
          "single-scatter source is Rayleigh, whose coefficient rises monotonically from red to "
          "blue -- nothing in the code names a colour");
    CHECK(side[2] > up[2],
          "and the horizon is BRIGHTER than the zenith, which surprises everyone once: the grazing "
          "path holds more air to scatter, and until extinction wins that is more light, not less");
  }
  {
    float towards[3], away[3];
    sky(std::cos(87.0f * 3.14159265358979f / 180.0f), 1.0f, evening, towards);
    sky(std::cos(87.0f * 3.14159265358979f / 180.0f), -1.0f, evening, away);
    Note("toward the setting sun: red", towards[0], "");
    Note("toward the setting sun: blue", towards[2], "");
    Note("away from it: red", away[0], "");
    CHECK(towards[0] > towards[2],
          "**THE SETTING SUN'S SIDE OF THE SKY IS RED**: at 88 deg sun zenith the light reaching "
          "the near-horizon view ray has crossed hundreds of kilometres of medium, and the "
          "transmittance table says what survives -- red, by the same coefficients that made noon "
          "blue");
    CHECK(towards[0] > away[0] * 1.5f,
          "and the sky AWAY from the sunset is dimmer in red by half again, which is the "
          "anisotropy a fixed-colour sky dome cannot have");
  }
  {
    float day[3], night[3];
    sky(std::cos(45.0f * 3.14159265358979f / 180.0f), 0.0f, noon, day);
    sky(std::cos(45.0f * 3.14159265358979f / 180.0f), 0.0f, -0.5f, night);
    Note("mid sky at noon: green", day[1], "");
    Note("mid sky with the sun 30 deg under: green", night[1], "");
    CHECK(night[1] < day[1] * 1.0e-3f,
          "and with the sun 30 deg below the horizon the same view ray carries under a thousandth "
          "of noon's light -- night falls out of the geometry, with no night flag anywhere");
  }

  Covers("I.18.5 the sky view ray reproduces the sky everyone knows -- blue noon, bright horizon, "
         "red sunset toward the sun, dark night -- from the medium's coefficients alone");
  return Report();
}
