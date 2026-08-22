#ifndef PARTICIPATINGMEDIUM_H
#define PARTICIPATINGMEDIUM_H

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace outshine::Render {

struct Medium {
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

inline constexpr uint32_t kTransmittanceLutWidth = 256;
inline constexpr uint32_t kTransmittanceLutHeight = 64;

inline constexpr int kTransmittanceSteps = 40;

inline constexpr float kMediumSampleSegment = 0.5f;

inline constexpr uint32_t kMultiScatterLutSize = 32;

inline constexpr int kMultiScatterSteps = 20;

inline constexpr int kMultiScatterGrid = 8;

inline constexpr float kMediumLuminanceSegment = 0.3f;

inline constexpr float kMediumGroundLiftKm = 0.01f;

inline constexpr uint32_t kSkyViewLutWidth = 192;

inline constexpr uint32_t kSkyViewLutHeight = 108;

inline constexpr int kSkyViewSteps = 30;

[[nodiscard]] inline float MediumTopReach(const Medium &medium, float radiusKm, float cosZenith) {
  const float under = radiusKm * radiusKm * (cosZenith * cosZenith - 1.0f) +
                      medium.TopRadiusKm * medium.TopRadiusKm;
  if (under < 0.0f) { return 0.0f; }
  return std::fmax(0.0f, -radiusKm * cosZenith + std::sqrt(under));
}

[[nodiscard]] inline float MediumGroundReach(const Medium &medium, float radiusKm, float cosZenith) {
  const float under = radiusKm * radiusKm * (cosZenith * cosZenith - 1.0f) +
                      medium.BottomRadiusKm * medium.BottomRadiusKm;
  if (under < 0.0f) { return -1.0f; }
  const float entry = -radiusKm * cosZenith - std::sqrt(under);
  return entry >= 0.0f ? entry : -1.0f;
}

[[nodiscard]] inline float MediumHeightAlong(const Medium &medium, float radiusKm, float cosZenith,
                                             float alongKm) {
  const float above = radiusKm - medium.BottomRadiusKm;
  const float raised = above * (radiusKm + medium.BottomRadiusKm) + alongKm * alongKm +
                       2.0f * radiusKm * alongKm * cosZenith;
  const float sampleKm =
      std::sqrt(radiusKm * radiusKm + alongKm * alongKm + 2.0f * radiusKm * alongKm * cosZenith);
  return raised / (sampleKm + medium.BottomRadiusKm);
}

inline void MediumExtinctionPerKm(const Medium &medium, float heightKm, float out[3]) {
  const float rayleigh = std::exp(-heightKm / medium.RayleighScaleHeightKm);
  const float mie = std::exp(-heightKm / medium.MieScaleHeightKm);
  const float tent = 1.0f - std::fabs(heightKm - medium.OzoneCentreKm) / medium.OzoneHalfWidthKm;
  const float ozone = std::fmin(1.0f, std::fmax(0.0f, tent));
  for (int channel = 0; channel < 3; ++channel) {
    out[channel] = rayleigh * medium.RayleighScatteringPerKm[channel] +
                   mie * medium.MieExtinctionPerKm +
                   ozone * medium.OzoneAbsorptionPerKm[channel];
  }
}

inline void MediumScatterExtinctPerKm(const Medium &medium, float heightKm, float scattering[3],
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
inline void MediumMultiScatterTexel(const Medium &medium, float unitU, float unitV,
                                    ToSun &&transmittanceToSun, float luminance[3],
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
    const float theta = 2.0f * 3.14159265358979f * ring;
    const float cosPhi = 1.0f - 2.0f * around;
    const float sinPhi = std::sqrt(std::fmax(0.0f, 1.0f - cosPhi * cosPhi));
    const float dir[3] = {std::cos(theta) * sinPhi, std::sin(theta) * sinPhi, cosPhi};
    const float cosView = dir[2];

    const float toGround = MediumGroundReach(medium, radiusKm, cosView);
    const float toTop = MediumTopReach(medium, radiusKm, cosView);
    const float span = toGround < 0.0f ? toTop : std::fmin(toTop, toGround);
    const float stride = span / (float)kMultiScatterSteps;

    double throughput[3] = {1.0, 1.0, 1.0};
    for (int step = 0; step < kMultiScatterSteps; ++step) {
      const float along = stride * ((float)step + kMediumLuminanceSegment);
      const float heightKm = MediumHeightAlong(medium, radiusKm, cosView, along);
      float scattering[3], extinction[3];
      MediumScatterExtinctPerKm(medium, heightKm, scattering, extinction);

      const float hereKm = heightKm + medium.BottomRadiusKm;

      const float sunDot = dir[2] * cosSun + dir[0] * sinSun;
      const float cosSunAt = (radiusKm * cosSun + along * sunDot) / hereKm;

      const float shadowed =
          MediumGroundReach(medium, hereKm - kMediumGroundLiftKm, cosSunAt) >= 0.0f ? 0.0f : 1.0f;
      float sun[3] = {0.0f, 0.0f, 0.0f};
      if (shadowed > 0.0f) { transmittanceToSun(hereKm, cosSunAt, sun); }

      for (int channel = 0; channel < 3; ++channel) {
        const double stepT = std::exp(-(double)extinction[channel] * (double)stride);
        const double source = (double)shadowed * (double)sun[channel] *
                              (double)scattering[channel] / (4.0 * 3.14159265358979);
        summedL[channel] += throughput[channel] * source * (1.0 - stepT) /
                            (double)extinction[channel];
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

[[nodiscard]] inline float RayleighPhase(float cosTheta) {
  return 3.0f / (16.0f * 3.14159265358979f) * (1.0f + cosTheta * cosTheta);
}

[[nodiscard]] inline float MiePhase(float g, float cosTheta) {
  const float k = 3.0f / (8.0f * 3.14159265358979f) * (1.0f - g * g) / (2.0f + g * g);
  return k * (1.0f + cosTheta * cosTheta) /
         std::pow(1.0f + g * g - 2.0f * g * cosTheta, 1.5f);
}

[[nodiscard]] inline float SubUvsToUnit(float u, float resolution) {
  return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f));
}

[[nodiscard]] inline float UnitToSubUvs(float u, float resolution) {
  return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f));
}

inline void SkyViewParams(const Medium &medium, float radiusKm, float unitU, float unitV,
                          float *cosView, float *lightViewCos) {
  const float u = SubUvsToUnit(unitU, (float)kSkyViewLutWidth);
  const float v = SubUvsToUnit(unitV, (float)kSkyViewLutHeight);

  const float toHorizon =
      std::sqrt(std::fmax(0.0f, radiusKm * radiusKm - medium.BottomRadiusKm * medium.BottomRadiusKm));
  const float beta = std::acos(std::fmin(1.0f, std::fmax(-1.0f, toHorizon / radiusKm)));
  const float zenithToHorizon = 3.14159265358979f - beta;
  if (v < 0.5f) {
    float coord = 1.0f - 2.0f * v;
    coord = 1.0f - coord * coord;
    *cosView = std::cos(zenithToHorizon * coord);
  } else {
    float coord = v * 2.0f - 1.0f;
    coord *= coord;
    *cosView = std::cos(zenithToHorizon + beta * coord);
  }
  *lightViewCos = -(u * u * 2.0f - 1.0f);
}

inline void SkyViewUv(const Medium &medium, float radiusKm, bool hitsGround, float cosView,
                      float lightViewCos, float *u, float *v) {
  const float toHorizon =
      std::sqrt(std::fmax(0.0f, radiusKm * radiusKm - medium.BottomRadiusKm * medium.BottomRadiusKm));
  const float beta = std::acos(std::fmin(1.0f, std::fmax(-1.0f, toHorizon / radiusKm)));
  const float zenithToHorizon = 3.14159265358979f - beta;
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
  *u = UnitToSubUvs(*u, (float)kSkyViewLutWidth);
  *v = UnitToSubUvs(*v, (float)kSkyViewLutHeight);
}

template <typename ToSun, typename Psi>
inline void MediumSkyRay(const Medium &medium, float radiusKm, float cosView, float lightViewCos,
                         float cosSunZenith, ToSun &&transmittanceToSun, Psi &&multiScattered,
                         float luminance[3]) {
  const float sinView = std::sqrt(std::fmax(0.0f, 1.0f - cosView * cosView));
  const float sinSun = std::sqrt(std::fmax(0.0f, 1.0f - cosSunZenith * cosSunZenith));
  const float dir[3] = {sinView * lightViewCos,
                        sinView * std::sqrt(std::fmax(0.0f, 1.0f - lightViewCos * lightViewCos)),
                        cosView};
  const float sun[3] = {sinSun, 0.0f, cosSunZenith};
  const float cosTheta = dir[0] * sun[0] + dir[2] * sun[2];
  const float phaseRay = RayleighPhase(cosTheta);
  const float phaseMie = MiePhase(medium.MiePhaseG, cosTheta);

  const float toGround = MediumGroundReach(medium, radiusKm, cosView);
  const float toTop = MediumTopReach(medium, radiusKm, cosView);
  const float span = toGround < 0.0f ? toTop : std::fmin(toTop, toGround);
  const float stride = span / (float)kSkyViewSteps;

  double summed[3] = {0.0, 0.0, 0.0};
  double throughput[3] = {1.0, 1.0, 1.0};
  for (int step = 0; step < kSkyViewSteps; ++step) {
    const float along = stride * ((float)step + kMediumLuminanceSegment);
    const float heightKm = MediumHeightAlong(medium, radiusKm, cosView, along);
    const float hereKm = heightKm + medium.BottomRadiusKm;
    const float rayleigh = std::exp(-heightKm / medium.RayleighScaleHeightKm);
    const float mie = std::exp(-heightKm / medium.MieScaleHeightKm);
    float extinction[3];
    MediumExtinctionPerKm(medium, heightKm, extinction);

    const float sunDot = dir[0] * sun[0] + dir[2] * sun[2];
    const float cosSunAt = (radiusKm * cosSunZenith + along * sunDot) / hereKm;
    const float shadowed =
        MediumGroundReach(medium, hereKm - kMediumGroundLiftKm, cosSunAt) >= 0.0f ? 0.0f : 1.0f;
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
    const float hereKm = std::sqrt(radiusKm * radiusKm + toGround * toGround +
                                   2.0f * radiusKm * toGround * cosView);
    const float sunDot = dir[0] * sun[0] + dir[2] * sun[2];
    const float cosSunAt = (radiusKm * cosSunZenith + toGround * sunDot) / hereKm;
    if (cosSunAt > 0.0f) {
      float toSunGround[3];
      transmittanceToSun(hereKm, cosSunAt, toSunGround);
      for (int channel = 0; channel < 3; ++channel) {
        summed[channel] += throughput[channel] * (double)toSunGround[channel] *
                           (double)cosSunAt * (double)medium.GroundAlbedo[channel] /
                           3.14159265358979;
      }
    }
  }
  for (int channel = 0; channel < 3; ++channel) { luminance[channel] = (float)summed[channel]; }
}

template <typename ToSun, typename Psi>
inline void MediumSkyIrradiance(const Medium &medium, float radiusKm, float cosSunZenith,
                                ToSun &&transmittanceToSun, Psi &&multiScattered,
                                float irradiance[3]) {
  double summed[3] = {0.0, 0.0, 0.0};
  for (int which = 0; which < kMultiScatterGrid * kMultiScatterGrid; ++which) {
    const float ring = ((float)(which / kMultiScatterGrid) + 0.5f) / (float)kMultiScatterGrid;
    const float around = ((float)(which % kMultiScatterGrid) + 0.5f) / (float)kMultiScatterGrid;
    const float azimuth = 2.0f * 3.14159265358979f * ring;

    const float cosView = std::sqrt(around);
    const float sinView = std::sqrt(std::fmax(0.0f, 1.0f - around));
    const float lightViewCos = std::cos(azimuth);
    (void)sinView;
    float luminance[3];
    MediumSkyRay(medium, radiusKm, cosView, lightViewCos, cosSunZenith, transmittanceToSun,
                 multiScattered, luminance);
    for (int channel = 0; channel < 3; ++channel) { summed[channel] += (double)luminance[channel]; }
  }

  for (int channel = 0; channel < 3; ++channel) {
    irradiance[channel] = (float)(summed[channel] * 3.14159265358979 /
                                  (double)(kMultiScatterGrid * kMultiScatterGrid));
  }
}

inline void MediumTransmittanceParams(const Medium &medium, float u, float v, float *radiusKm,
                                      float *cosZenith) {
  const float span = std::sqrt(std::fmax(0.0f, medium.TopRadiusKm * medium.TopRadiusKm -
                                                   medium.BottomRadiusKm * medium.BottomRadiusKm));
  const float ground = span * v;
  *radiusKm = std::sqrt(ground * ground + medium.BottomRadiusKm * medium.BottomRadiusKm);

  const float shortest = medium.TopRadiusKm - *radiusKm;
  const float longest = ground + span;
  const float reach = shortest + u * (longest - shortest);
  *cosZenith = reach == 0.0f ? 1.0f
                             : (span * span - ground * ground - reach * reach) /
                                   (2.0f * *radiusKm * reach);
  *cosZenith = std::fmin(1.0f, std::fmax(-1.0f, *cosZenith));
}

inline void MediumTransmittanceUv(const Medium &medium, float radiusKm, float cosZenith, float *u,
                                  float *v) {
  const float span = std::sqrt(std::fmax(0.0f, medium.TopRadiusKm * medium.TopRadiusKm -
                                                   medium.BottomRadiusKm * medium.BottomRadiusKm));
  const float ground =
      std::sqrt(std::fmax(0.0f, radiusKm * radiusKm - medium.BottomRadiusKm * medium.BottomRadiusKm));
  const float reach = MediumTopReach(medium, radiusKm, cosZenith);
  const float shortest = medium.TopRadiusKm - radiusKm;
  const float longest = ground + span;
  *u = (reach - shortest) / (longest - shortest);
  *v = ground / span;
}

inline void MediumTransmittance(const Medium &medium, float radiusKm, float cosZenith, int steps,
                                float out[3]) {
  const float toGround = MediumGroundReach(medium, radiusKm, cosZenith);
  const float toTop = MediumTopReach(medium, radiusKm, cosZenith);
  const float span = toGround < 0.0f ? toTop : std::fmin(toTop, toGround);

  double depth[3] = {0.0, 0.0, 0.0};
  const float stride = span / (float)steps;
  for (int step = 0; step < steps; ++step) {
    const float along = stride * ((float)step + kMediumSampleSegment);

    float extinction[3];
    MediumExtinctionPerKm(medium, MediumHeightAlong(medium, radiusKm, cosZenith, along), extinction);
    for (int channel = 0; channel < 3; ++channel) {
      depth[channel] += (double)extinction[channel] * (double)stride;
    }
  }
  for (int channel = 0; channel < 3; ++channel) { out[channel] = (float)std::exp(-depth[channel]); }
}

[[nodiscard]] inline std::string ParticipatingMediumMsl(void) {
  return R"(
struct Medium {
  float bottomRadiusKm; float topRadiusKm; float rayleighScaleHeightKm; float mieScaleHeightKm;
  packed_float3 rayleighScatteringPerKm; float mieScatteringPerKm;
  packed_float3 ozoneAbsorptionPerKm; float mieExtinctionPerKm;
  float ozoneCentreKm; float ozoneHalfWidthKm; float miePhaseG; float pad;
  packed_float3 groundAlbedo; float pad2;
};

static inline float mediumTopReach(constant Medium &medium, float radiusKm, float cosZenith) {
  float under = radiusKm * radiusKm * (cosZenith * cosZenith - 1.0) +
                medium.topRadiusKm * medium.topRadiusKm;
  if (under < 0.0) { return 0.0; }
  return max(0.0, -radiusKm * cosZenith + sqrt(under));
}

static inline float mediumGroundReach(constant Medium &medium, float radiusKm, float cosZenith) {
  float under = radiusKm * radiusKm * (cosZenith * cosZenith - 1.0) +
                medium.bottomRadiusKm * medium.bottomRadiusKm;
  if (under < 0.0) { return -1.0; }
  float entry = -radiusKm * cosZenith - sqrt(under);
  return entry >= 0.0 ? entry : -1.0;
}

static inline float mediumHeightAlong(constant Medium &medium, float radiusKm, float cosZenith,
                                      float alongKm) {
  float above = radiusKm - medium.bottomRadiusKm;
  float raised = above * (radiusKm + medium.bottomRadiusKm) + alongKm * alongKm +
                 2.0 * radiusKm * alongKm * cosZenith;
  float sampleKm = sqrt(radiusKm * radiusKm + alongKm * alongKm + 2.0 * radiusKm * alongKm * cosZenith);
  return raised / (sampleKm + medium.bottomRadiusKm);
}

static inline float3 mediumExtinctionPerKm(constant Medium &medium, float heightKm) {
  float rayleigh = exp(-heightKm / medium.rayleighScaleHeightKm);
  float mie = exp(-heightKm / medium.mieScaleHeightKm);
  float tent = 1.0 - fabs(heightKm - medium.ozoneCentreKm) / medium.ozoneHalfWidthKm;
  float ozone = clamp(tent, 0.0, 1.0);
  return rayleigh * float3(medium.rayleighScatteringPerKm) + mie * medium.mieExtinctionPerKm +
         ozone * float3(medium.ozoneAbsorptionPerKm);
}

static inline float2 mediumTransmittanceUv(constant Medium &medium, float radiusKm,
                                           float cosZenith) {
  float span = sqrt(max(0.0, medium.topRadiusKm * medium.topRadiusKm -
                             medium.bottomRadiusKm * medium.bottomRadiusKm));
  float ground =
      sqrt(max(0.0, radiusKm * radiusKm - medium.bottomRadiusKm * medium.bottomRadiusKm));
  float reach = mediumTopReach(medium, radiusKm, cosZenith);
  float shortest = medium.topRadiusKm - radiusKm;
  float longest = ground + span;
  return float2((reach - shortest) / (longest - shortest), ground / span);
}

static inline void mediumScatterExtinctPerKm(constant Medium &medium, float heightKm,
                                             thread float3 &scattering,
                                             thread float3 &extinction) {
  float rayleigh = exp(-heightKm / medium.rayleighScaleHeightKm);
  float mie = exp(-heightKm / medium.mieScaleHeightKm);
  float tent = 1.0 - fabs(heightKm - medium.ozoneCentreKm) / medium.ozoneHalfWidthKm;
  float ozone = clamp(tent, 0.0, 1.0);
  scattering = rayleigh * float3(medium.rayleighScatteringPerKm) + mie * medium.mieScatteringPerKm;
  extinction = rayleigh * float3(medium.rayleighScatteringPerKm) +
               mie * medium.mieExtinctionPerKm + ozone * float3(medium.ozoneAbsorptionPerKm);
}

static inline void mediumTransmittanceParams(constant Medium &medium, float u, float v,
                                             thread float &radiusKm, thread float &cosZenith) {
  float span = sqrt(max(0.0, medium.topRadiusKm * medium.topRadiusKm -
                             medium.bottomRadiusKm * medium.bottomRadiusKm));
  float ground = span * v;
  radiusKm = sqrt(ground * ground + medium.bottomRadiusKm * medium.bottomRadiusKm);
  float shortest = medium.topRadiusKm - radiusKm;
  float longest = ground + span;
  float reach = shortest + u * (longest - shortest);
  cosZenith = reach == 0.0 ? 1.0
                           : (span * span - ground * ground - reach * reach) /
                                 (2.0 * radiusKm * reach);
  cosZenith = clamp(cosZenith, -1.0, 1.0);
}

static inline void mediumMultiScatterTexel(constant Medium &medium, float unitU, float unitV,
                                           texture2d<float> transmittance, sampler lut,
                                           uint steps, uint grid, float segment, float liftKm,
                                           thread float3 &luminance, thread float3 &transfer) {
  float cosSun = unitU * 2.0 - 1.0;
  float sinSun = sqrt(max(0.0, 1.0 - cosSun * cosSun));
  float radiusKm = medium.bottomRadiusKm + liftKm +
                   unitV * (medium.topRadiusKm - medium.bottomRadiusKm - liftKm);
  float3 summedL = float3(0.0);
  float3 summedF = float3(0.0);
  for (uint which = 0u; which < grid * grid; which = which + 1u) {
    float ring = (float(which / grid) + 0.5) / float(grid);
    float around = (float(which % grid) + 0.5) / float(grid);
    float theta = 2.0 * 3.14159265358979 * ring;
    float cosPhi = 1.0 - 2.0 * around;
    float sinPhi = sqrt(max(0.0, 1.0 - cosPhi * cosPhi));
    float3 dir = float3(cos(theta) * sinPhi, sin(theta) * sinPhi, cosPhi);
    float cosView = dir.z;
    float toGround = mediumGroundReach(medium, radiusKm, cosView);
    float toTop = mediumTopReach(medium, radiusKm, cosView);
    float span = toGround < 0.0 ? toTop : min(toTop, toGround);
    float stride = span / float(steps);
    float3 throughput = float3(1.0);
    for (uint step = 0u; step < steps; step = step + 1u) {
      float along = stride * (float(step) + segment);
      float heightKm = mediumHeightAlong(medium, radiusKm, cosView, along);
      float3 scattering;
      float3 extinction;
      mediumScatterExtinctPerKm(medium, heightKm, scattering, extinction);
      float hereKm = heightKm + medium.bottomRadiusKm;
      float sunDot = dir.z * cosSun + dir.x * sinSun;
      float cosSunAt = (radiusKm * cosSun + along * sunDot) / hereKm;
      float shadowed = mediumGroundReach(medium, hereKm - liftKm, cosSunAt) >= 0.0 ? 0.0 : 1.0;
      float3 sun = float3(0.0);
      if (shadowed > 0.0) {
        sun = transmittance
                  .sample(lut, mediumTransmittanceUv(medium, hereKm, cosSunAt), level(0.0))
                  .rgb;
      }
      float3 stepT = exp(-extinction * stride);
      float3 source = shadowed * sun * scattering / (4.0 * 3.14159265358979);
      summedL += throughput * source * (1.0 - stepT) / extinction;
      summedF += throughput * scattering * (1.0 - stepT) / extinction;
      throughput *= stepT;
    }
  }
  luminance = summedL / float(grid * grid);
  transfer = summedF / float(grid * grid);
}

static inline float rayleighPhase(float cosTheta) {
  return 3.0 / (16.0 * 3.14159265358979) * (1.0 + cosTheta * cosTheta);
}

static inline float miePhase(float g, float cosTheta) {
  float k = 3.0 / (8.0 * 3.14159265358979) * (1.0 - g * g) / (2.0 + g * g);
  return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5);
}

static inline float subUvsToUnit(float u, float resolution) {
  return (u - 0.5 / resolution) * (resolution / (resolution - 1.0));
}

static inline float unitToSubUvs(float u, float resolution) {
  return (u + 0.5 / resolution) * (resolution / (resolution + 1.0));
}

static inline void skyViewParams(constant Medium &medium, float radiusKm, float unitU, float unitV,
                                 float widthPx, float heightPx, thread float &cosView,
                                 thread float &lightViewCos) {
  float u = subUvsToUnit(unitU, widthPx);
  float v = subUvsToUnit(unitV, heightPx);
  float toHorizon =
      sqrt(max(0.0, radiusKm * radiusKm - medium.bottomRadiusKm * medium.bottomRadiusKm));
  float beta = acos(clamp(toHorizon / radiusKm, -1.0, 1.0));
  float zenithToHorizon = 3.14159265358979 - beta;
  if (v < 0.5) {
    float coord = 1.0 - 2.0 * v;
    coord = 1.0 - coord * coord;
    cosView = cos(zenithToHorizon * coord);
  } else {
    float coord = v * 2.0 - 1.0;
    coord *= coord;
    cosView = cos(zenithToHorizon + beta * coord);
  }
  lightViewCos = -(u * u * 2.0 - 1.0);
}

static inline float2 skyViewUv(constant Medium &medium, float radiusKm, bool hitsGround,
                               float cosView, float lightViewCos, float widthPx, float heightPx) {
  float toHorizon =
      sqrt(max(0.0, radiusKm * radiusKm - medium.bottomRadiusKm * medium.bottomRadiusKm));
  float beta = acos(clamp(toHorizon / radiusKm, -1.0, 1.0));
  float zenithToHorizon = 3.14159265358979 - beta;
  float v;
  if (!hitsGround) {
    float coord = acos(clamp(cosView, -1.0, 1.0)) / zenithToHorizon;
    coord = 1.0 - sqrt(max(0.0, 1.0 - coord));
    v = coord * 0.5;
  } else {
    float coord = (acos(clamp(cosView, -1.0, 1.0)) - zenithToHorizon) / beta;
    v = sqrt(max(0.0, coord)) * 0.5 + 0.5;
  }
  float u = sqrt(max(0.0, -lightViewCos * 0.5 + 0.5));
  return float2(unitToSubUvs(u, widthPx), unitToSubUvs(v, heightPx));
}

static inline float3 mediumSkyRay(constant Medium &medium, float radiusKm, float cosView,
                                  float lightViewCos, float cosSunZenith,
                                  texture2d<float> transmittance, texture2d<float> multiScatter,
                                  sampler lut, uint steps, float segment, float liftKm) {
  float sinView = sqrt(max(0.0, 1.0 - cosView * cosView));
  float sinSun = sqrt(max(0.0, 1.0 - cosSunZenith * cosSunZenith));
  float3 dir = float3(sinView * lightViewCos,
                      sinView * sqrt(max(0.0, 1.0 - lightViewCos * lightViewCos)), cosView);
  float3 sun = float3(sinSun, 0.0, cosSunZenith);
  float cosTheta = dot(dir, sun);
  float phaseRay = rayleighPhase(cosTheta);
  float phaseMie = miePhase(medium.miePhaseG, cosTheta);

  float toGround = mediumGroundReach(medium, radiusKm, cosView);
  float toTop = mediumTopReach(medium, radiusKm, cosView);
  float span = toGround < 0.0 ? toTop : min(toTop, toGround);
  float stride = span / float(steps);

  float3 summed = float3(0.0);
  float3 throughput = float3(1.0);
  for (uint step = 0u; step < steps; step = step + 1u) {
    float along = stride * (float(step) + segment);
    float heightKm = mediumHeightAlong(medium, radiusKm, cosView, along);
    float hereKm = heightKm + medium.bottomRadiusKm;
    float rayleigh = exp(-heightKm / medium.rayleighScaleHeightKm);
    float mie = exp(-heightKm / medium.mieScaleHeightKm);
    float3 extinction = mediumExtinctionPerKm(medium, heightKm);

    float cosSunAt = (radiusKm * cosSunZenith + along * cosTheta) / hereKm;
    float shadowed = mediumGroundReach(medium, hereKm - liftKm, cosSunAt) >= 0.0 ? 0.0 : 1.0;
    float3 toSun = float3(0.0);
    if (shadowed > 0.0) {
      toSun = transmittance.sample(lut, mediumTransmittanceUv(medium, hereKm, cosSunAt), level(0.0))
                  .rgb;
    }
    float2 psiUv = float2(cosSunAt * 0.5 + 0.5,
                          (hereKm - medium.bottomRadiusKm) /
                              (medium.topRadiusKm - medium.bottomRadiusKm));
    float psiRes = float(multiScatter.get_width());
    psiUv = float2(unitToSubUvs(psiUv.x, psiRes), unitToSubUvs(psiUv.y, psiRes));
    float3 psi = multiScatter.sample(lut, psiUv, level(0.0)).rgb;

    float3 scatterRay = rayleigh * float3(medium.rayleighScatteringPerKm);
    float3 scatterMie = float3(mie * medium.mieScatteringPerKm);
    float3 source = shadowed * toSun * (scatterMie * phaseMie + scatterRay * phaseRay) +
                    psi * (scatterRay + scatterMie);
    float3 stepT = exp(-extinction * stride);
    summed += throughput * source * (1.0 - stepT) / extinction;
    throughput *= stepT;
  }
  if (toGround >= 0.0) {
    float hereKm = sqrt(radiusKm * radiusKm + toGround * toGround +
                        2.0 * radiusKm * toGround * cosView);
    float sunDot = dot(dir, sun);
    float cosSunAt = (radiusKm * cosSunZenith + toGround * sunDot) / hereKm;
    if (cosSunAt > 0.0) {
      float3 toSunGround =
          transmittance.sample(lut, mediumTransmittanceUv(medium, hereKm, cosSunAt), level(0.0))
              .rgb;
      summed += throughput * toSunGround * cosSunAt * float3(medium.groundAlbedo) /
                3.14159265358979;
    }
  }
  return summed;
}

static inline float3 mediumTransmittance(constant Medium &medium, float radiusKm, float cosZenith,
                                         uint steps, float segment) {
  float toGround = mediumGroundReach(medium, radiusKm, cosZenith);
  float toTop = mediumTopReach(medium, radiusKm, cosZenith);
  float span = toGround < 0.0 ? toTop : min(toTop, toGround);
  float3 depth = float3(0.0);
  float stride = span / float(steps);
  for (uint step = 0u; step < steps; step = step + 1u) {
    float along = stride * (float(step) + segment);
    depth += mediumExtinctionPerKm(medium, mediumHeightAlong(medium, radiusKm, cosZenith, along)) *
             stride;
  }
  return exp(-depth);
}
)";
}

}
#endif
