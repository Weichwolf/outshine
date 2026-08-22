#include <cmath>
#include <cstdio>

#include "Check.h"

#include "ParticipatingMedium.h"

using outshine::Render::kMediumSampleSegment;
using outshine::Render::kTransmittanceLutHeight;
using outshine::Render::kTransmittanceLutWidth;
using outshine::Render::kTransmittanceSteps;
using outshine::Render::Medium;
using outshine::Render::MediumExtinctionPerKm;
using outshine::Render::mediumGroundReach;
using outshine::Render::mediumTopReach;
using outshine::Render::MediumTransmittance;
using outshine::Render::mediumTransmittanceParams;
using outshine::Render::MediumTransmittanceUv;

namespace {

double LayerDepth(double coefficient, double scaleHeightKm, double throughKm) {
  return coefficient * scaleHeightKm * (1.0 - std::exp(-throughKm / scaleHeightKm));
}

void AnalyticZenithDepth(const Medium &medium, double out[3]) {
  const double throughKm = (double)medium.TopRadiusKm - (double)medium.BottomRadiusKm;
  for (int channel = 0; channel < 3; ++channel) {
    out[channel] =
        LayerDepth(medium.RayleighScatteringPerKm[channel], medium.RayleighScaleHeightKm, throughKm) +
        LayerDepth(medium.MieExtinctionPerKm, medium.MieScaleHeightKm, throughKm) +
        (double)medium.OzoneAbsorptionPerKm[channel] * (double)medium.OzoneHalfWidthKm;
  }
}

double MarchedDepth(const Medium &medium, int steps, int channel) {
  float got[3];
  MediumTransmittance(medium, medium.BottomRadiusKm, 1.0f, steps, got);
  return -std::log((double)got[channel]);
}

const char *const kChannel[3] = {"red", "green", "blue"};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Medium medium;

  Note("the medium's planet radius", medium.BottomRadiusKm, "km");
  Note("the medium's top radius", medium.TopRadiusKm, "km");
  CHECK(medium.TopRadiusKm - medium.BottomRadiusKm == 100.0f,
        "**THE MEDIUM IS 100 KM DEEP, WHICH IS THE KARMAN LINE AND NOT A ROUND NUMBER SOMEBODY "
        "LIKED.** Bruneton's coefficients were integrated over a wavelength power spectrum against "
        "exactly this pair of radii, so taking one without the other takes a mechanism without its "
        "assumption. 6360 km is the medium's planet and not the geodesy's: the horizon dip a "
        "2 m eye sees differs from WGS84's by 0.00006 degrees, which is 1/750 of a pixel at 720p");

  double analytic[3];
  AnalyticZenithDepth(medium, analytic);

  for (int channel = 0; channel < 3; ++channel) {
    Note(kChannel[channel], std::exp(-analytic[channel]), "transmittance, analytic at the zenith");
  }
  CHECK(std::exp(-analytic[0]) > std::exp(-analytic[1]) &&
            std::exp(-analytic[1]) > std::exp(-analytic[2]),
        "**A ZENITH SUN LOSES ITS BLUE FIRST, and that is the whole of why a sky is a sky.** The "
        "Rayleigh coefficient runs 0.005802 : 0.013558 : 0.033100 per km at 680, 550 and 440 nm, "
        "so the analytic transmittance falls monotonically from red to blue with no code involved");

  CHECK(mediumGroundReach(medium, medium.BottomRadiusKm, 1.0f) < 0.0f ||
            mediumGroundReach(medium, medium.BottomRadiusKm, 1.0f) == 0.0f,
        "a ray leaving the ground straight up never meets the ground again ahead of it");
  Note("how far a zenith ray travels", mediumTopReach(medium, medium.BottomRadiusKm, 1.0f), "km");
  CHECK_NEAR(mediumTopReach(medium, medium.BottomRadiusKm, 1.0f), 100.0, 1e-3, "km",
             "**and it travels exactly the depth of the medium, because it is radial.** That is why "
             "this one ray has an ORACLE the march cannot borrow from: its optical depth is a "
             "closed integral of an exponential, and a spherical geometry contributes nothing to it");

  Note("steps the march takes", (double)kTransmittanceSteps, "samples");
  for (int channel = 0; channel < 3; ++channel) {
    const double marched = MarchedDepth(medium, kTransmittanceSteps, channel);
    const double got = std::exp(-marched);
    const double want = std::exp(-analytic[channel]);
    std::printf("NOTE %s: analytic depth %.9f, marched %.9f, transmittance error %.3e (%.4f %%)\n",
                kChannel[channel], analytic[channel], marched, got - want,
                100.0 * (got - want) / want);
    CHECK_NEAR(got, want, 2.0e-3, "transmittance",
               "**THE MARCH LANDS ON THE CLOSED FORM.** 2e-3 is half a step of an 8-bit "
               "quantisation (1/255 = 3.9e-3), so an error this size cannot move a pixel of the "
               "picture this LUT feeds -- and the population is the one ray whose answer is known "
               "without the march");
  }

  {
    const double coarse = MarchedDepth(medium, 10, 2);
    const double fine = MarchedDepth(medium, kTransmittanceSteps, 2);
    const double settled = MarchedDepth(medium, 4096, 2);
    Note("blue depth at 10 steps", coarse, "");
    Note("blue depth at 40 steps", fine, "");
    Note("blue depth at 4096 steps", settled, "");
    std::printf("NOTE the step count buys: 10 -> %.3e, 40 -> %.3e against the settled march\n",
                std::fabs(coarse - settled), std::fabs(fine - settled));
    CHECK(std::fabs(fine - settled) < std::fabs(coarse - settled) * 0.2,
          "**AND 40 IS A MEASUREMENT RATHER THAN THE REFERENCE'S NUMBER COPIED.** The source says "
          "'can go as low as 10 samples but energy lost starts to be visible'; this is the 'visible' "
          "made into a number, and 40 removes at least four fifths of what 10 leaves behind");
  }

  {
    Medium biased = medium;
    float mid[3], skewed[3];
    MediumTransmittance(biased, biased.BottomRadiusKm, 1.0f, kTransmittanceSteps, mid);

    double skewedDepth[3] = {0, 0, 0};
    {
      const double stride = 100.0 / (double)kTransmittanceSteps;
      for (int step = 0; step < kTransmittanceSteps; ++step) {
        const double along = stride * ((double)step + 0.3);
        float extinction[3];
        MediumExtinctionPerKm(biased, (float)along, extinction);
        for (int channel = 0; channel < 3; ++channel) {
          skewedDepth[channel] += (double)extinction[channel] * stride;
        }
      }
    }
    for (int channel = 0; channel < 3; ++channel) { skewed[channel] = (float)std::exp(-skewedDepth[channel]); }

    const double midError = std::fabs(-std::log((double)mid[2]) - analytic[2]);
    const double skewError = std::fabs(skewedDepth[2] - analytic[2]);
    Note("blue depth error, sample at 0.5 of the step", midError, "");
    Note("blue depth error, sample at 0.3 of the step", skewError, "");
    CHECK(midError < skewError,
          "**THE SAMPLE SITS AT THE MIDDLE OF ITS STEP, and that is a DEVIATION from the reference "
          "with its reason beside it.** Hillaire's march samples at 0.3 because the SAME function "
          "integrates scattered luminance, where the transmittance weight leans the average toward "
          "the near end of the step. A pure optical depth has no such weight: the midpoint rule is "
          "second order there and 0.3 is first order, so copying 0.3 into this pass would import a "
          "bias that belongs to a different integral");
    CHECK(kMediumSampleSegment == 0.5f, "and the constant says so");
  }

  {
    size_t walked = 0;
    double worst = 0.0;
    double worstU = 0.0, worstV = 0.0;
    for (uint32_t y = 0; y < kTransmittanceLutHeight; ++y) {
      for (uint32_t x = 0; x < kTransmittanceLutWidth; ++x) {
        const float u = ((float)x + 0.5f) / (float)kTransmittanceLutWidth;
        const float v = ((float)y + 0.5f) / (float)kTransmittanceLutHeight;
        float radiusKm = 0.0f, cosZenith = 0.0f;
        mediumTransmittanceParams(medium, u, v, radiusKm, cosZenith);
        float back = 0.0f, backV = 0.0f;
        MediumTransmittanceUv(medium, radiusKm, cosZenith, &back, &backV);
        const double drift = std::fmax(std::fabs((double)back - u), std::fabs((double)backV - v));
        if (drift > worst) {
          worst = drift;
          worstU = u;
          worstV = v;
        }
        ++walked;
      }
    }
    Note("texels of the transmittance lut walked", (double)walked, "texels");
    std::printf("NOTE the worst round trip sits at u=%.6f v=%.6f and drifts %.3e\n", worstU, worstV,
                worst);
    CHECK(walked == (size_t)kTransmittanceLutWidth * kTransmittanceLutHeight,
          "every texel of the 256x64 table is walked, which is the population this drift is "
          "measured over rather than a sample of it");
    Note("one texel of u", 1.0 / (double)kTransmittanceLutWidth, "");
    CHECK(worst < 0.1 / (double)kTransmittanceLutWidth,
          "**THE PARAMETERISATION IS A BIJECTION, texel for texel.** Bruneton's mapping bends the "
          "table toward the horizon, where transmittance changes fastest; a forward map that did "
          "not invert its own inverse would put the sun's colour at the wrong altitude and nothing "
          "downstream could tell. **The bound is a TENTH OF A TEXEL and not a round decimal**: the "
          "map is float32 on both sides because its twin is a shader, and near the horizon it "
          "resolves a distance through the difference of two squared planet radii -- so the "
          "question a bijection has to answer is whether the drift can move a lookup, and a tenth "
          "of a texel is the honest form of no");
  }

  {
    float zenith[3], horizon[3];
    MediumTransmittance(medium, medium.BottomRadiusKm, 1.0f, kTransmittanceSteps, zenith);
    float radiusKm = 0.0f, cosZenith = 0.0f;
    const float grazingU = ((float)kTransmittanceLutWidth - 0.5f) / (float)kTransmittanceLutWidth;
    const float lowestV = 0.5f / (float)kTransmittanceLutHeight;
    mediumTransmittanceParams(medium, grazingU, lowestV, radiusKm, cosZenith);
    MediumTransmittance(medium, radiusKm, cosZenith, kTransmittanceSteps, horizon);
    Note("the lowest texel row sits at", (radiusKm - medium.BottomRadiusKm) * 1000.0, "m above the ground");
    Note("the grazing ray's cos zenith there", cosZenith, "");
    Note("how far that ray travels", mediumTopReach(medium, radiusKm, cosZenith), "km");
    Note("blue at the zenith", zenith[2], "transmittance");
    Note("blue along the horizon", horizon[2], "transmittance");
    Note("red along the horizon", horizon[0], "transmittance");
    CHECK(cosZenith < 0.0f && cosZenith > -1.0e-2f,
          "**THE POPULATION IS THE TEXEL CENTRES THE TABLE ACTUALLY STORES, not the corner of its "
          "domain.** The last column and the first row are a LIMIT the mapping approaches: at "
          "exactly u=1, v=0 the ray is exactly tangent from exactly sea level, float32 rounds it a "
          "billionth below the horizon, and the ground stops it after zero metres for a "
          "transmittance of one. That corner is unreachable through a sampler -- a clamped fetch "
          "lands on a texel centre -- so measuring there would be measuring something the picture "
          "never asks for");
    CHECK(horizon[2] < zenith[2] * 0.02f,
          "**AND THE HORIZON IS AT LEAST FIFTY TIMES DARKER IN BLUE, which is sunset.** A grazing "
          "ray crosses 1132 km of medium against the zenith's 100, so the same coefficients that "
          "cost a quarter of the blue overhead cost essentially all of it at the horizon");
    CHECK(horizon[0] > horizon[2] * 10.0f,
          "and what survives there is red by more than a factor of ten, which is why a low sun is "
          "red and not merely dim");

    float tangent[3];
    mediumTransmittanceParams(medium, 1.0f, 0.0f, radiusKm, cosZenith);
    MediumTransmittance(medium, radiusKm, cosZenith, kTransmittanceSteps, tangent);
    Note("blue at the exactly tangent corner", tangent[2], "transmittance");
    Note("how far the tangent ray gets before the ground",
         mediumGroundReach(medium, radiusKm, cosZenith) * 1000.0, "m");
    CHECK(tangent[2] > 0.999999f && mediumGroundReach(medium, radiusKm, cosZenith) >= 0.0f &&
              mediumGroundReach(medium, radiusKm, cosZenith) * 1000.0f < 1.0f,
          "**AND THE CORNER IS CHECKED RATHER THAN AVOIDED**, because a limit nobody wrote down is "
          "a limit somebody rediscovers. The ground stops the tangent ray at zero metres and the "
          "medium takes nothing from it -- which is arithmetic, not a defect, and the reference "
          "keeps a 10 m planet radius offset for exactly this reason. The ray gets under a metre, "
          "which costs blue a transmittance of 2.4e-7 -- the arithmetic agreeing with itself");
  }

  {
    float low[3], high[3];
    MediumExtinctionPerKm(medium, 0.0f, low);
    MediumExtinctionPerKm(medium, 60.0f, high);
    Note("blue extinction at sea level", low[2], "per km");
    Note("blue extinction at 60 km", high[2], "per km");
    CHECK(high[2] < low[2] * 1.0e-3f,
          "the medium thins out with height, and by 60 km -- above the ozone's tent and seven "
          "Rayleigh scale heights up -- a thousandth of the sea level extinction is left");
    Medium clear = medium;
    for (float &channel : clear.OzoneAbsorptionPerKm) { channel = 0.0f; }
    const auto ozoneAt = [&](float heightKm) {
      float full[3], without[3];
      MediumExtinctionPerKm(medium, heightKm, full);
      MediumExtinctionPerKm(clear, heightKm, without);
      return full[1] - without[1];
    };
    Note("green ozone term at the tent's peak", ozoneAt(medium.OzoneCentreKm), "per km");
    Note("green ozone term at 10 km", ozoneAt(medium.OzoneCentreKm - medium.OzoneHalfWidthKm),
         "per km");
    Note("green ozone term at 40 km", ozoneAt(medium.OzoneCentreKm + medium.OzoneHalfWidthKm),
         "per km");
    Note("green ozone term half way up the tent", ozoneAt(17.5f), "per km");
    CHECK_NEAR(ozoneAt(medium.OzoneCentreKm), medium.OzoneAbsorptionPerKm[1], 1e-9, "per km",
               "the tent reaches exactly its coefficient at 25 km");
    CHECK(ozoneAt(medium.OzoneCentreKm - medium.OzoneHalfWidthKm) == 0.0f &&
              ozoneAt(medium.OzoneCentreKm + medium.OzoneHalfWidthKm) == 0.0f,
          "**AND OZONE IS A TENT AND NOT AN EXPONENTIAL**, peaking at 25 km and vanishing at 10 "
          "and 40 -- written as the tent it is rather than as the two linear layers with "
          "hand-solved constants the reference carries, because a constant somebody solved by hand "
          "is where a sign goes missing");
    CHECK(medium.MieExtinctionPerKm > medium.MieScatteringPerKm,
          "and Mie extinguishes more than it scatters, because aerosol absorbs -- 0.004440 against "
          "0.003996 per km, a difference that only shows up as a slightly darker haze");
    CHECK_NEAR(ozoneAt(17.5f), 0.5 * medium.OzoneAbsorptionPerKm[1], 1e-9, "per km",
               "**and it is LINEAR on the way up, which is what makes it a tent rather than a "
               "bump**: half way from 10 km to 25 km it carries exactly half its coefficient. The "
               "ozone term is measured by DIFFERENCING two media rather than read off the total, "
               "because Rayleigh at 10 km is larger than ozone at its own peak -- an instrument "
               "that read the total would have called this tent a hole");
  }

  Covers("I.18.1 the engine carries a participating medium whose transmittance is Bruneton's "
         "parameterisation over Hillaire's chain, and its zenith ray is checked against the closed "
         "form rather than against another implementation");
  return Report();
}
