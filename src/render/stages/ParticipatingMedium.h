#ifndef OUTSHINE_RENDER_STAGES_PARTICIPATINGMEDIUM_H
#define OUTSHINE_RENDER_STAGES_PARTICIPATINGMEDIUM_H

#include <numbers>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace outshine::Render {

struct alignas(16) Medium {
  float BottomRadiusKm = 6360.0f;
  float TopRadiusKm = 6460.0f;
  float RayleighScaleHeightKm = 8.0f;
  float MieScaleHeightKm = 1.2f;

  float RayleighScatteringPerKm[3] = {0.005802f, 0.013558f, 0.033100f};
  float MieScatteringPerKm = 0.003996f;

  float OzoneAbsorptionPerKm[3] = {0.000650f, 0.001881f, 0.000085f};
  float MieExtinctionPerKm = 0.004440f;

  float OzoneCentreKm = 25.0f;
  float OzoneHalfWidthKm = 15.0f;
  float MiePhaseG = 0.8f;
  float Pad = 0.0f;

  float GroundAlbedo[3] = {0.10f, 0.13f, 0.07f};
  float Pad2 = 0.0f;
};

static_assert(sizeof(Medium) == 80, "the medium is five float4 rows a device can bind unpadded");
static_assert(alignof(Medium) == 16,
              "and the rows start on a 128-bit boundary -- the upload is a vector copy, and the "
              "declaration costs zero padding (80 = 5 x 16)");

namespace medium_core {
using ::outshine::Render::Medium;

inline double max(double a, double b) {
  return a > b ? a : b;
}

inline double clamp(double x, double lo, double hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

using std::acos;
using std::cos;
using std::exp;
using std::fabs;
using std::pow;
using std::sqrt;
#define MEDIUM_CONST const
#define MEDIUM_THREAD
#define OUTSHINE_PI std::numbers::pi
#include "MediumCore.h"
#undef MEDIUM_CONST
#undef MEDIUM_THREAD
#undef OUTSHINE_PI
} // namespace medium_core

using medium_core::mediumGroundReach;
using medium_core::mediumHeightAlong;
using medium_core::mediumTopReach;
using medium_core::mediumTransmittanceParams;
using medium_core::miePhase;
using medium_core::rayleighPhase;
using medium_core::skyViewParams;
using medium_core::subUvsToUnit;
using medium_core::unitToSubUvs;

inline constexpr uint32_t kTransmittanceLutWidth = 256;
inline constexpr uint32_t kTransmittanceLutHeight = 64;

inline constexpr int kTransmittanceSteps = 40;

inline constexpr float kMediumSampleSegment = 0.5f;

inline constexpr uint32_t kMultiScatterLutSize = 32;

inline constexpr int kMultiScatterSteps = 20;

inline constexpr int kMultiScatterGrid = 8;

inline constexpr float kMediumLuminanceSegment = 0.3f;

inline constexpr float kMediumGroundLiftKm = 0.01f;

inline constexpr float kSunHalfAngleRad = 4.6542e-3f;

inline constexpr uint32_t kSkyViewLutWidth = 192;

inline constexpr uint32_t kSkyViewLutHeight = 108;

inline constexpr int kSkyViewSteps = 30;

inline void MediumExtinctionPerKm(const Medium &medium, float heightKm, float out[3]) {
  const float rayleigh = std::exp(-heightKm / medium.RayleighScaleHeightKm);
  const float mie = std::exp(-heightKm / medium.MieScaleHeightKm);
  const float tent = 1.0f - std::fabs(heightKm - medium.OzoneCentreKm) / medium.OzoneHalfWidthKm;
  const float ozone = std::fmin(1.0f, std::fmax(0.0f, tent));
  for (int channel = 0; channel < 3; ++channel) {
    out[channel] = rayleigh * medium.RayleighScatteringPerKm[channel] +
                   mie * medium.MieExtinctionPerKm + ozone * medium.OzoneAbsorptionPerKm[channel];
  }
}

inline void MediumScatterExtinctPerKm(const Medium &medium,
                                      float heightKm,
                                      float scattering[3],
                                      float extinction[3]) {
  const float rayleigh = std::exp(-heightKm / medium.RayleighScaleHeightKm);
  const float mie = std::exp(-heightKm / medium.MieScaleHeightKm);
  const float tent = 1.0f - std::fabs(heightKm - medium.OzoneCentreKm) / medium.OzoneHalfWidthKm;
  const float ozone = std::fmin(1.0f, std::fmax(0.0f, tent));
  for (int channel = 0; channel < 3; ++channel) {
    scattering[channel] =
        rayleigh * medium.RayleighScatteringPerKm[channel] + mie * medium.MieScatteringPerKm;
    extinction[channel] = rayleigh * medium.RayleighScatteringPerKm[channel] +
                          mie * medium.MieExtinctionPerKm +
                          ozone * medium.OzoneAbsorptionPerKm[channel];
  }
}

template <typename ToSun>
inline void MediumMultiScatterTexel(const Medium &medium,
                                    float unitU,
                                    float unitV,
                                    ToSun &&transmittanceToSun,
                                    float luminance[3],
                                    float transfer[3]) {
  const float cosSun = unitU * 2.0f - 1.0f;
  const float sinSun = std::sqrt(std::fmax(0.0f, 1.0f - cosSun * cosSun));
  const float radiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm +
                         unitV * (medium.TopRadiusKm - medium.BottomRadiusKm - kMediumGroundLiftKm);

  double summedL[3] = {0.0, 0.0, 0.0};
  double summedF[3] = {0.0, 0.0, 0.0};
  for (int which = 0; which < kMultiScatterGrid * kMultiScatterGrid; ++which) {
    const float ring = ((float)(which / kMultiScatterGrid) + 0.5f) / (float)kMultiScatterGrid;
    const float around = ((float)(which % kMultiScatterGrid) + 0.5f) / (float)kMultiScatterGrid;
    const float theta = 2.0f * std::numbers::pi_v<float> * ring;
    const float cosPhi = 1.0f - 2.0f * around;
    const float sinPhi = std::sqrt(std::fmax(0.0f, 1.0f - cosPhi * cosPhi));
    const float dir[3] = {std::cos(theta) * sinPhi, std::sin(theta) * sinPhi, cosPhi};
    const float cosView = dir[2];

    const float toGround = mediumGroundReach(medium, radiusKm, cosView);
    const float toTop = mediumTopReach(medium, radiusKm, cosView);
    const float span = toGround < 0.0f ? toTop : std::fmin(toTop, toGround);
    const float stride = span / (float)kMultiScatterSteps;

    double throughput[3] = {1.0, 1.0, 1.0};
    for (int step = 0; step < kMultiScatterSteps; ++step) {
      const float along = stride * ((float)step + kMediumLuminanceSegment);
      const float heightKm = mediumHeightAlong(medium, radiusKm, cosView, along);
      float scattering[3], extinction[3];
      MediumScatterExtinctPerKm(medium, heightKm, scattering, extinction);

      const float hereKm = heightKm + medium.BottomRadiusKm;

      const float sunDot = dir[2] * cosSun + dir[0] * sinSun;
      const float cosSunAt = (radiusKm * cosSun + along * sunDot) / hereKm;

      const float shadowed =
          mediumGroundReach(medium, hereKm - kMediumGroundLiftKm, cosSunAt) >= 0.0f ? 0.0f : 1.0f;
      float sun[3] = {0.0f, 0.0f, 0.0f};
      if (shadowed > 0.0f) { transmittanceToSun(hereKm, cosSunAt, sun); }

      for (int channel = 0; channel < 3; ++channel) {
        const double stepT = std::exp(-(double)extinction[channel] * (double)stride);
        const double source = (double)shadowed * (double)sun[channel] *
                              (double)scattering[channel] / (4.0 * std::numbers::pi);
        summedL[channel] +=
            throughput[channel] * source * (1.0 - stepT) / (double)extinction[channel];
        summedF[channel] += throughput[channel] * (double)scattering[channel] * (1.0 - stepT) /
                            (double)extinction[channel];
        throughput[channel] *= stepT;
      }
    }
  }
  for (int channel = 0; channel < 3; ++channel) {
    luminance[channel] =
        (float)(summedL[channel] / (double)(kMultiScatterGrid * kMultiScatterGrid));
    transfer[channel] = (float)(summedF[channel] / (double)(kMultiScatterGrid * kMultiScatterGrid));
  }
}

inline void SkyViewUv(const Medium &medium,
                      float radiusKm,
                      bool hitsGround,
                      float cosView,
                      float lightViewCos,
                      float *u,
                      float *v) {
  const float toHorizon = std::sqrt(
      std::fmax(0.0f, radiusKm * radiusKm - medium.BottomRadiusKm * medium.BottomRadiusKm));
  const float beta = std::acos(std::fmin(1.0f, std::fmax(-1.0f, toHorizon / radiusKm)));
  const float zenithToHorizon = std::numbers::pi_v<float> - beta;
  if (!hitsGround) {
    float coord = std::acos(std::fmin(1.0f, std::fmax(-1.0f, cosView))) / zenithToHorizon;
    coord = 1.0f - std::sqrt(std::fmax(0.0f, 1.0f - coord));
    *v = coord * 0.5f;
  } else {
    const float coord =
        (std::acos(std::fmin(1.0f, std::fmax(-1.0f, cosView))) - zenithToHorizon) / beta;
    *v = std::sqrt(std::fmax(0.0f, coord)) * 0.5f + 0.5f;
  }
  *u = std::sqrt(std::fmax(0.0f, -lightViewCos * 0.5f + 0.5f));
  *u = unitToSubUvs(*u, (float)kSkyViewLutWidth);
  *v = unitToSubUvs(*v, (float)kSkyViewLutHeight);
}

template <typename ToSun, typename Psi>
inline void MediumSkyRay(const Medium &medium,
                         float radiusKm,
                         float cosView,
                         float lightViewCos,
                         float cosSunZenith,
                         ToSun &&transmittanceToSun,
                         Psi &&multiScattered,
                         float luminance[3]) {
  const float sinView = std::sqrt(std::fmax(0.0f, 1.0f - cosView * cosView));
  const float sinSun = std::sqrt(std::fmax(0.0f, 1.0f - cosSunZenith * cosSunZenith));
  const float dir[3] = {sinView * lightViewCos,
                        sinView * std::sqrt(std::fmax(0.0f, 1.0f - lightViewCos * lightViewCos)),
                        cosView};
  const float sun[3] = {sinSun, 0.0f, cosSunZenith};
  const float cosTheta = dir[0] * sun[0] + dir[2] * sun[2];
  const float phaseRay = rayleighPhase(cosTheta);
  const float phaseMie = miePhase(medium.MiePhaseG, cosTheta);

  const float toGround = mediumGroundReach(medium, radiusKm, cosView);
  const float toTop = mediumTopReach(medium, radiusKm, cosView);
  const float span = toGround < 0.0f ? toTop : std::fmin(toTop, toGround);
  const float stride = span / (float)kSkyViewSteps;

  double summed[3] = {0.0, 0.0, 0.0};
  double throughput[3] = {1.0, 1.0, 1.0};
  for (int step = 0; step < kSkyViewSteps; ++step) {
    const float along = stride * ((float)step + kMediumLuminanceSegment);
    const float heightKm = mediumHeightAlong(medium, radiusKm, cosView, along);
    const float hereKm = heightKm + medium.BottomRadiusKm;
    const float rayleigh = std::exp(-heightKm / medium.RayleighScaleHeightKm);
    const float mie = std::exp(-heightKm / medium.MieScaleHeightKm);
    float extinction[3];
    MediumExtinctionPerKm(medium, heightKm, extinction);

    const float sunDot = dir[0] * sun[0] + dir[2] * sun[2];
    const float cosSunAt = (radiusKm * cosSunZenith + along * sunDot) / hereKm;
    const float shadowed =
        mediumGroundReach(medium, hereKm - kMediumGroundLiftKm, cosSunAt) >= 0.0f ? 0.0f : 1.0f;
    float toSun[3] = {0.0f, 0.0f, 0.0f};
    if (shadowed > 0.0f) { transmittanceToSun(hereKm, cosSunAt, toSun); }
    float psi[3] = {0.0f, 0.0f, 0.0f};
    multiScattered(hereKm, cosSunAt, psi);

    for (int channel = 0; channel < 3; ++channel) {
      const float scatterRay = rayleigh * medium.RayleighScatteringPerKm[channel];
      const float scatterMie = mie * medium.MieScatteringPerKm;
      const double source =
          (double)shadowed * (double)toSun[channel] *
              ((double)scatterMie * (double)phaseMie + (double)scatterRay * (double)phaseRay) +
          (double)psi[channel] * ((double)scatterRay + (double)scatterMie);
      const double stepT = std::exp(-(double)extinction[channel] * (double)stride);
      summed[channel] += throughput[channel] * source * (1.0 - stepT) / (double)extinction[channel];
      throughput[channel] *= stepT;
    }
  }
  if (toGround >= 0.0f) {
    const float hereKm =
        std::sqrt(radiusKm * radiusKm + toGround * toGround + 2.0f * radiusKm * toGround * cosView);
    const float sunDot = dir[0] * sun[0] + dir[2] * sun[2];
    const float cosSunAt = (radiusKm * cosSunZenith + toGround * sunDot) / hereKm;
    if (cosSunAt > 0.0f) {
      float toSunGround[3];
      transmittanceToSun(hereKm, cosSunAt, toSunGround);
      for (int channel = 0; channel < 3; ++channel) {
        summed[channel] += throughput[channel] * (double)toSunGround[channel] * (double)cosSunAt *
                           (double)medium.GroundAlbedo[channel] / std::numbers::pi;
      }
    }
  }
  for (int channel = 0; channel < 3; ++channel) { luminance[channel] = (float)summed[channel]; }
}

template <typename ToSun, typename Psi>
inline void MediumSkyIrradiance(const Medium &medium,
                                float radiusKm,
                                float cosSunZenith,
                                ToSun &&transmittanceToSun,
                                Psi &&multiScattered,
                                float irradiance[3]) {
  double summed[3] = {0.0, 0.0, 0.0};
  for (int which = 0; which < kMultiScatterGrid * kMultiScatterGrid; ++which) {
    const float ring = ((float)(which / kMultiScatterGrid) + 0.5f) / (float)kMultiScatterGrid;
    const float around = ((float)(which % kMultiScatterGrid) + 0.5f) / (float)kMultiScatterGrid;
    const float azimuth = 2.0f * std::numbers::pi_v<float> * ring;

    const float cosView = std::sqrt(around);
    const float sinView = std::sqrt(std::fmax(0.0f, 1.0f - around));
    const float lightViewCos = std::cos(azimuth);
    (void)sinView;
    float luminance[3];
    MediumSkyRay(medium,
                 radiusKm,
                 cosView,
                 lightViewCos,
                 cosSunZenith,
                 transmittanceToSun,
                 multiScattered,
                 luminance);
    for (int channel = 0; channel < 3; ++channel) { summed[channel] += (double)luminance[channel]; }
  }

  for (int channel = 0; channel < 3; ++channel) {
    irradiance[channel] = (float)(summed[channel] * std::numbers::pi /
                                  (double)(kMultiScatterGrid * kMultiScatterGrid));
  }
}

inline void
MediumTransmittanceUv(const Medium &medium, float radiusKm, float cosZenith, float *u, float *v) {
  const float span = std::sqrt(std::fmax(0.0f,
                                         medium.TopRadiusKm * medium.TopRadiusKm -
                                             medium.BottomRadiusKm * medium.BottomRadiusKm));
  const float ground = std::sqrt(
      std::fmax(0.0f, radiusKm * radiusKm - medium.BottomRadiusKm * medium.BottomRadiusKm));
  const float reach = mediumTopReach(medium, radiusKm, cosZenith);
  const float shortest = medium.TopRadiusKm - radiusKm;
  const float longest = ground + span;
  *u = (reach - shortest) / (longest - shortest);
  *v = ground / span;
}

inline void MediumTransmittance(
    const Medium &medium, float radiusKm, float cosZenith, int steps, float out[3]) {
  const float toGround = mediumGroundReach(medium, radiusKm, cosZenith);
  const float toTop = mediumTopReach(medium, radiusKm, cosZenith);
  const float span = toGround < 0.0f ? toTop : std::fmin(toTop, toGround);

  double depth[3] = {0.0, 0.0, 0.0};
  const float stride = span / (float)steps;
  for (int step = 0; step < steps; ++step) {
    const float along = stride * ((float)step + kMediumSampleSegment);

    float extinction[3];
    MediumExtinctionPerKm(
        medium, mediumHeightAlong(medium, radiusKm, cosZenith, along), extinction);
    for (int channel = 0; channel < 3; ++channel) {
      depth[channel] += (double)extinction[channel] * (double)stride;
    }
  }
  for (int channel = 0; channel < 3; ++channel) { out[channel] = (float)std::exp(-depth[channel]); }
}

[[nodiscard]] bool ParticipatingMediumMsl(std::string &into, std::string &error);

} // namespace outshine::Render
#endif
