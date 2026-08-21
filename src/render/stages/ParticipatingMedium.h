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
};

static_assert(sizeof(Medium) == 64, "the medium is four float4 rows a device can bind unpadded");

inline constexpr uint32_t kTransmittanceLutWidth = 256;
inline constexpr uint32_t kTransmittanceLutHeight = 64;

inline constexpr int kTransmittanceSteps = 40;

inline constexpr float kMediumSampleSegment = 0.5f;

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
