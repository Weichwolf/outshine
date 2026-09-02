#ifndef OUTSHINE_RENDER_STAGES_PARTICIPATINGMEDIUM_H
#define OUTSHINE_RENDER_STAGES_PARTICIPATINGMEDIUM_H

#include <algorithm>

#include "math/Vec3.h"
#include <numbers>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace outshine::Render {

constexpr size_t kMediumBytes = 5 * 4 * sizeof(float);

struct alignas(16) Medium {
  float BottomRadiusKm;
  float TopRadiusKm;
  float RayleighScaleHeightKm;
  float MieScaleHeightKm;

  Vec3f RayleighScatteringPerKm;
  float MieScatteringPerKm;

  Vec3f OzoneAbsorptionPerKm;
  float MieExtinctionPerKm;

  float OzoneCentreKm;
  float OzoneHalfWidthKm;
  float MiePhaseG;
  float Pad;

  Vec3f GroundAlbedo;
  float Pad2;
};

inline constexpr Medium kEarthAir = {
    .BottomRadiusKm = 6360.0f,
    .TopRadiusKm = 6460.0f,
    .RayleighScaleHeightKm = 8.0f,
    .MieScaleHeightKm = 1.2f,
    .RayleighScatteringPerKm = {0.005802f, 0.013558f, 0.033100f},
    .MieScatteringPerKm = 0.039960f,
    .OzoneAbsorptionPerKm = {0.000650f, 0.001881f, 0.000085f},
    .MieExtinctionPerKm = 0.044400f,
    .OzoneCentreKm = 25.0f,
    .OzoneHalfWidthKm = 15.0f,
    .MiePhaseG = 0.8f,
    .Pad = 0.0f,
    .GroundAlbedo = {0.10f, 0.13f, 0.07f},
    .Pad2 = 0.0f,
};

static_assert(sizeof(Medium) == kMediumBytes,
              "the medium is five float4 rows a device can bind unpadded");
static_assert(alignof(Medium) == 16,
              "and the rows start on a 128-bit boundary -- the upload is a vector copy, and the "
              "declaration costs zero padding (80 = 5 x 16)");

namespace medium_core {
using ::outshine::Render::Medium;

inline float max(float a, float b) {
  return a > b ? a : b;
}

inline float clamp(float x, float lo, float hi) {
  return std::clamp(x, lo, hi);
}

using std::acos;
using std::cos;
using std::exp;
using std::fabs;
using std::pow;
using std::sqrt;
#define MEDIUM_CONST const
#define MEDIUM_THREAD
#define OUTSHINE_PI std::numbers::pi_v<float>
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

inline void MediumExtinctionPerKm(const Medium &medium, float heightKm, Vec3f &out) {
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
                                      Vec3f &scattering,
                                      Vec3f &extinction) {
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
                                    Vec3f &luminance,
                                    Vec3f &transfer) {
  const float cosSun = unitU * 2.0f - 1.0f;
  const float sinSun = std::sqrt(std::fmax(0.0f, 1.0f - cosSun * cosSun));
  const float radiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm +
                         unitV * (medium.TopRadiusKm - medium.BottomRadiusKm - kMediumGroundLiftKm);

  Vec3 summedL = {0.0, 0.0, 0.0};
  Vec3 summedF = {0.0, 0.0, 0.0};
  for (int which = 0; which < kMultiScatterGrid * kMultiScatterGrid; ++which) {
    const float ring = (static_cast<float>(which / kMultiScatterGrid) + 0.5f) /
                       static_cast<float>(kMultiScatterGrid);
    const float around = (static_cast<float>(which % kMultiScatterGrid) + 0.5f) /
                         static_cast<float>(kMultiScatterGrid);
    const float theta = 2.0f * std::numbers::pi_v<float> * ring;
    const float cosPhi = 1.0f - 2.0f * around;
    const float sinPhi = std::sqrt(std::fmax(0.0f, 1.0f - cosPhi * cosPhi));
    const Vec3f dir = {std::cos(theta) * sinPhi, std::sin(theta) * sinPhi, cosPhi};
    const float cosView = dir[2];

    const float toGround = mediumGroundReach(medium, radiusKm, cosView);
    const float toTop = mediumTopReach(medium, radiusKm, cosView);
    const float span = toGround < 0.0f ? toTop : std::fmin(toTop, toGround);
    const float stride = span / static_cast<float>(kMultiScatterSteps);

    Vec3 throughput = {1.0, 1.0, 1.0};
    for (int step = 0; step < kMultiScatterSteps; ++step) {
      const float along = stride * (static_cast<float>(step) + kMediumLuminanceSegment);
      const float heightKm = mediumHeightAlong(medium, radiusKm, cosView, along);
      Vec3f scattering;
      Vec3f extinction;
      MediumScatterExtinctPerKm(medium, heightKm, scattering, extinction);

      const float hereKm = heightKm + medium.BottomRadiusKm;

      const float sunDot = dir[2] * cosSun + dir[0] * sinSun;
      const float cosSunAt = (radiusKm * cosSun + along * sunDot) / hereKm;

      const float shadowed =
          mediumGroundReach(medium, hereKm - kMediumGroundLiftKm, cosSunAt) >= 0.0f ? 0.0f : 1.0f;
      Vec3f sun = {0.0f, 0.0f, 0.0f};
      if (shadowed > 0.0f) { transmittanceToSun(hereKm, cosSunAt, sun); }

      for (int channel = 0; channel < 3; ++channel) {
        const double stepT =
            std::exp(-static_cast<double>(extinction[channel]) * static_cast<double>(stride));
        const double source = static_cast<double>(shadowed) * static_cast<double>(sun[channel]) *
                              static_cast<double>(scattering[channel]) / (4.0 * std::numbers::pi);
        summedL[channel] +=
            throughput[channel] * source * (1.0 - stepT) / static_cast<double>(extinction[channel]);
        summedF[channel] += throughput[channel] * static_cast<double>(scattering[channel]) *
                            (1.0 - stepT) / static_cast<double>(extinction[channel]);
        throughput[channel] *= stepT;
      }
    }
  }
  for (int channel = 0; channel < 3; ++channel) {
    luminance[channel] = static_cast<float>(
        summedL[channel] / static_cast<double>(kMultiScatterGrid * kMultiScatterGrid));
    transfer[channel] = static_cast<float>(
        summedF[channel] / static_cast<double>(kMultiScatterGrid * kMultiScatterGrid));
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
  *u = unitToSubUvs(*u, static_cast<float>(kSkyViewLutWidth));
  *v = unitToSubUvs(*v, static_cast<float>(kSkyViewLutHeight));
}

template <typename ToSun, typename Psi>
inline void MediumSkyRay(const Medium &medium,
                         float radiusKm,
                         float cosView,
                         float lightViewCos,
                         float cosSunZenith,
                         ToSun &&transmittanceToSun,
                         Psi &&multiScattered,
                         Vec3f &luminance) {
  const float sinView = std::sqrt(std::fmax(0.0f, 1.0f - cosView * cosView));
  const float sinSun = std::sqrt(std::fmax(0.0f, 1.0f - cosSunZenith * cosSunZenith));
  const Vec3f dir = {sinView * lightViewCos,
                     sinView * std::sqrt(std::fmax(0.0f, 1.0f - lightViewCos * lightViewCos)),
                     cosView};
  const Vec3f sun = {sinSun, 0.0f, cosSunZenith};
  const float cosTheta = dir[0] * sun[0] + dir[2] * sun[2];
  const float phaseRay = rayleighPhase(cosTheta);
  const float phaseMie = miePhase(medium.MiePhaseG, cosTheta);

  const float toGround = mediumGroundReach(medium, radiusKm, cosView);
  const float toTop = mediumTopReach(medium, radiusKm, cosView);
  const float span = toGround < 0.0f ? toTop : std::fmin(toTop, toGround);
  const float stride = span / static_cast<float>(kSkyViewSteps);

  Vec3 summed = {0.0, 0.0, 0.0};
  Vec3 throughput = {1.0, 1.0, 1.0};
  for (int step = 0; step < kSkyViewSteps; ++step) {
    const float along = stride * (static_cast<float>(step) + kMediumLuminanceSegment);
    const float heightKm = mediumHeightAlong(medium, radiusKm, cosView, along);
    const float hereKm = heightKm + medium.BottomRadiusKm;
    const float rayleigh = std::exp(-heightKm / medium.RayleighScaleHeightKm);
    const float mie = std::exp(-heightKm / medium.MieScaleHeightKm);
    Vec3f extinction;
    MediumExtinctionPerKm(medium, heightKm, extinction);

    const float sunDot = dir[0] * sun[0] + dir[2] * sun[2];
    const float cosSunAt = (radiusKm * cosSunZenith + along * sunDot) / hereKm;
    const float shadowed =
        mediumGroundReach(medium, hereKm - kMediumGroundLiftKm, cosSunAt) >= 0.0f ? 0.0f : 1.0f;
    Vec3f toSun = {0.0f, 0.0f, 0.0f};
    if (shadowed > 0.0f) { transmittanceToSun(hereKm, cosSunAt, toSun); }
    Vec3f psi = {0.0f, 0.0f, 0.0f};
    multiScattered(hereKm, cosSunAt, psi);

    for (int channel = 0; channel < 3; ++channel) {
      const float scatterRay = rayleigh * medium.RayleighScatteringPerKm[channel];
      const float scatterMie = mie * medium.MieScatteringPerKm;
      const double source = static_cast<double>(shadowed) * static_cast<double>(toSun[channel]) *
                                (static_cast<double>(scatterMie) * static_cast<double>(phaseMie) +
                                 static_cast<double>(scatterRay) * static_cast<double>(phaseRay)) +
                            static_cast<double>(psi[channel]) *
                                (static_cast<double>(scatterRay) + static_cast<double>(scatterMie));
      const double stepT =
          std::exp(-static_cast<double>(extinction[channel]) * static_cast<double>(stride));
      summed[channel] +=
          throughput[channel] * source * (1.0 - stepT) / static_cast<double>(extinction[channel]);
      throughput[channel] *= stepT;
    }
  }
  if (toGround >= 0.0f) {
    const float hereKm =
        std::sqrt(radiusKm * radiusKm + toGround * toGround + 2.0f * radiusKm * toGround * cosView);
    const float sunDot = dir[0] * sun[0] + dir[2] * sun[2];
    const float cosSunAt = (radiusKm * cosSunZenith + toGround * sunDot) / hereKm;
    if (cosSunAt > 0.0f) {
      Vec3f toSunGround;
      transmittanceToSun(hereKm, cosSunAt, toSunGround);
      for (int channel = 0; channel < 3; ++channel) {
        summed[channel] += throughput[channel] * static_cast<double>(toSunGround[channel]) *
                           static_cast<double>(cosSunAt) *
                           static_cast<double>(medium.GroundAlbedo[channel]) / std::numbers::pi;
      }
    }
  }
  for (int channel = 0; channel < 3; ++channel) {
    luminance[channel] = static_cast<float>(summed[channel]);
  }
}

template <typename ToSun, typename Psi>
inline void MediumSkyIrradiance(const Medium &medium,
                                float radiusKm,
                                float cosSunZenith,
                                ToSun &&transmittanceToSun,
                                Psi &&multiScattered,
                                Vec3f &irradiance) {
  Vec3 summed = {0.0, 0.0, 0.0};
  for (int which = 0; which < kMultiScatterGrid * kMultiScatterGrid; ++which) {
    const float ring = (static_cast<float>(which / kMultiScatterGrid) + 0.5f) /
                       static_cast<float>(kMultiScatterGrid);
    const float around = (static_cast<float>(which % kMultiScatterGrid) + 0.5f) /
                         static_cast<float>(kMultiScatterGrid);
    const float azimuth = 2.0f * std::numbers::pi_v<float> * ring;

    const float cosView = std::sqrt(around);
    const float sinView = std::sqrt(std::fmax(0.0f, 1.0f - around));
    const float lightViewCos = std::cos(azimuth);
    (void)sinView;
    Vec3f luminance;
    MediumSkyRay(medium,
                 radiusKm,
                 cosView,
                 lightViewCos,
                 cosSunZenith,
                 transmittanceToSun,
                 multiScattered,
                 luminance);
    for (int channel = 0; channel < 3; ++channel) {
      summed[channel] += static_cast<double>(luminance[channel]);
    }
  }

  for (int channel = 0; channel < 3; ++channel) {
    irradiance[channel] =
        static_cast<float>(summed[channel] * std::numbers::pi /
                           static_cast<double>(kMultiScatterGrid * kMultiScatterGrid));
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

inline void
MediumTransmittance(const Medium &medium, float radiusKm, float cosZenith, int steps, Vec3f &out) {
  const float toGround = mediumGroundReach(medium, radiusKm, cosZenith);
  const float toTop = mediumTopReach(medium, radiusKm, cosZenith);
  const float span = toGround < 0.0f ? toTop : std::fmin(toTop, toGround);

  Vec3 depth = {0.0, 0.0, 0.0};
  const float stride = span / static_cast<float>(steps);
  for (int step = 0; step < steps; ++step) {
    const float along = stride * (static_cast<float>(step) + kMediumSampleSegment);

    Vec3f extinction;
    MediumExtinctionPerKm(
        medium, mediumHeightAlong(medium, radiusKm, cosZenith, along), extinction);
    for (int channel = 0; channel < 3; ++channel) {
      depth[channel] += static_cast<double>(extinction[channel]) * static_cast<double>(stride);
    }
  }
  for (int channel = 0; channel < 3; ++channel) {
    out[channel] = static_cast<float>(std::exp(-depth[channel]));
  }
}

[[nodiscard]] bool ParticipatingMediumMsl(std::string &into, std::string &error);

} // namespace outshine::Render
#endif
