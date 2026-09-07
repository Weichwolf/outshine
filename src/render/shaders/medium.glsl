#ifndef OUTSHINE_MEDIUM_GLSL
#define OUTSHINE_MEDIUM_GLSL
#include "mediumLayout.glsl"
#define OUTSHINE_PI acos(-1.0)
#define MEDIUM_ARG Medium
#define MEDIUM_INLINE
#include "../stages/MediumCore.h"
#undef MEDIUM_ARG
#undef MEDIUM_INLINE
#define MEDIUM_UINT(name, value) const uint name = value;
#define MEDIUM_INT(name, value) const uint name = value;
#define MEDIUM_FLOAT(name, value) const float name = value;
#include "../stages/MediumConstants.inc"
#undef MEDIUM_UINT
#undef MEDIUM_INT
#undef MEDIUM_FLOAT

vec3 mediumExtinctionPerKm(Medium medium, float heightKm) {
  float rayleigh = exp(-heightKm / medium.RayleighScaleHeightKm);
  float mie = exp(-heightKm / medium.MieScaleHeightKm);
  float tent = 1.0 - abs(heightKm - medium.OzoneCentreKm) / medium.OzoneHalfWidthKm;
  float ozone = clamp(tent, 0.0, 1.0);
  return rayleigh * vec3(medium.RayleighScatteringPerKm) + mie * medium.MieExtinctionPerKm +
         ozone * vec3(medium.OzoneAbsorptionPerKm);
}

vec2 mediumTransmittanceUv(Medium medium, MediumLook look) {
  float span = sqrt(max(0.0, medium.TopRadiusKm * medium.TopRadiusKm -
                             medium.BottomRadiusKm * medium.BottomRadiusKm));
  float ground = sqrt(
      max(0.0, look.RadiusKm * look.RadiusKm - medium.BottomRadiusKm * medium.BottomRadiusKm));
  float reach = mediumTopReach(medium, look.RadiusKm, look.CosZenith);
  float shortest = medium.TopRadiusKm - look.RadiusKm;
  float longest = ground + span;
  return vec2((reach - shortest) / (longest - shortest), ground / span);
}

void mediumScatterExtinctPerKm(Medium medium, float heightKm,
                                             out vec3 scattering,
                                             out vec3 extinction) {
  float rayleigh = exp(-heightKm / medium.RayleighScaleHeightKm);
  float mie = exp(-heightKm / medium.MieScaleHeightKm);
  float tent = 1.0 - abs(heightKm - medium.OzoneCentreKm) / medium.OzoneHalfWidthKm;
  float ozone = clamp(tent, 0.0, 1.0);
  scattering = rayleigh * vec3(medium.RayleighScatteringPerKm) + mie * medium.MieScatteringPerKm;
  extinction = rayleigh * vec3(medium.RayleighScatteringPerKm) +
               mie * medium.MieExtinctionPerKm + ozone * vec3(medium.OzoneAbsorptionPerKm);
}

void mediumMultiScatterTexel(Medium medium, float unitU, float unitV,
                                           sampler2D transmittance,
                                           uint steps, uint grid, float segment, float liftKm,
                                           out vec3 luminance, out vec3 transfer) {
  float cosSun = unitU * 2.0 - 1.0;
  float sinSun = sqrt(max(0.0, 1.0 - cosSun * cosSun));
  float radiusKm = medium.BottomRadiusKm + liftKm +
                   unitV * (medium.TopRadiusKm - medium.BottomRadiusKm - liftKm);
  vec3 summedL = vec3(0.0);
  vec3 summedF = vec3(0.0);
  for (uint which = 0u; which < grid * grid; which = which + 1u) {
    float ring = (float(which / grid) + 0.5) / float(grid);
    float around = (float(which % grid) + 0.5) / float(grid);
    float theta = 2.0 * OUTSHINE_PI * ring;
    float cosPhi = 1.0 - 2.0 * around;
    float sinPhi = sqrt(max(0.0, 1.0 - cosPhi * cosPhi));
    vec3 dir = vec3(cos(theta) * sinPhi, sin(theta) * sinPhi, cosPhi);
    float cosView = dir.z;
    float toGround = mediumGroundReach(medium, radiusKm, cosView);
    float toTop = mediumTopReach(medium, radiusKm, cosView);
    float span = toGround < 0.0 ? toTop : min(toTop, toGround);
    float stride = span / float(steps);
    vec3 throughput = vec3(1.0);
    for (uint step = 0u; step < steps; step = step + 1u) {
      float along = stride * (float(step) + segment);
      float heightKm = mediumHeightAlong(medium, radiusKm, cosView, along);
      vec3 scattering;
      vec3 extinction;
      mediumScatterExtinctPerKm(medium, heightKm, scattering, extinction);
      float hereKm = heightKm + medium.BottomRadiusKm;
      float sunDot = dir.z * cosSun + dir.x * sinSun;
      float cosSunAt = (radiusKm * cosSun + along * sunDot) / hereKm;
      float shadowed = mediumGroundReach(medium, hereKm - liftKm, cosSunAt) >= 0.0 ? 0.0 : 1.0;
      vec3 sun = vec3(0.0);
      if (shadowed > 0.0) {
        sun = textureLod(transmittance, mediumTransmittanceUv(medium, MediumLook(hereKm, cosSunAt)), 0.0)
                  .rgb;
      }
      vec3 stepT = exp(-extinction * stride);
      vec3 source = shadowed * sun * scattering / (4.0 * OUTSHINE_PI);
      summedL += throughput * source * (1.0 - stepT) / extinction;
      summedF += throughput * scattering * (1.0 - stepT) / extinction;
      throughput *= stepT;
    }
  }
  luminance = summedL / float(grid * grid);
  transfer = summedF / float(grid * grid);
}

vec3 mediumSkyRay(Medium medium, float radiusKm, SkyViewLook look,
                                  float cosSunZenith,
                                  sampler2D transmittance, sampler2D multiScatter,
                                  uint steps, float segment, float liftKm) {
  float cosView = look.CosView;
  float lightViewCos = look.LightViewCos;
  float sinView = sqrt(max(0.0, 1.0 - cosView * cosView));
  float sinSun = sqrt(max(0.0, 1.0 - cosSunZenith * cosSunZenith));
  vec3 dir = vec3(sinView * lightViewCos,
                      sinView * sqrt(max(0.0, 1.0 - lightViewCos * lightViewCos)), cosView);
  vec3 sun = vec3(sinSun, 0.0, cosSunZenith);
  float cosTheta = dot(dir, sun);
  float phaseRay = rayleighPhase(cosTheta);
  float phaseMie = miePhase(medium.MiePhaseG, cosTheta);

  float toGround = mediumGroundReach(medium, radiusKm, cosView);
  float toTop = mediumTopReach(medium, radiusKm, cosView);
  float span = toGround < 0.0 ? toTop : min(toTop, toGround);
  float stride = span / float(steps);

  vec3 summed = vec3(0.0);
  vec3 throughput = vec3(1.0);
  for (uint step = 0u; step < steps; step = step + 1u) {
    float along = stride * (float(step) + segment);
    float heightKm = mediumHeightAlong(medium, radiusKm, cosView, along);
    float hereKm = heightKm + medium.BottomRadiusKm;
    float rayleigh = exp(-heightKm / medium.RayleighScaleHeightKm);
    float mie = exp(-heightKm / medium.MieScaleHeightKm);
    vec3 extinction = mediumExtinctionPerKm(medium, heightKm);

    float cosSunAt = (radiusKm * cosSunZenith + along * cosTheta) / hereKm;
    float shadowed = mediumGroundReach(medium, hereKm - liftKm, cosSunAt) >= 0.0 ? 0.0 : 1.0;
    vec3 toSun = vec3(0.0);
    if (shadowed > 0.0) {
      toSun = textureLod(transmittance, mediumTransmittanceUv(medium, MediumLook(hereKm, cosSunAt)), 0.0)
                  .rgb;
    }
    vec2 psiUv = vec2(cosSunAt * 0.5 + 0.5,
                          (hereKm - medium.BottomRadiusKm) /
                              (medium.TopRadiusKm - medium.BottomRadiusKm));
    float psiRes = float(textureSize(multiScatter, 0).x);
    psiUv = vec2(unitToSubUvs(psiUv.x, psiRes), unitToSubUvs(psiUv.y, psiRes));
    vec3 psi = textureLod(multiScatter, psiUv, 0.0).rgb;

    vec3 scatterRay = rayleigh * vec3(medium.RayleighScatteringPerKm);
    vec3 scatterMie = vec3(mie * medium.MieScatteringPerKm);
    vec3 source = shadowed * toSun * (scatterMie * phaseMie + scatterRay * phaseRay) +
                    psi * (scatterRay + scatterMie);
    vec3 stepT = exp(-extinction * stride);
    summed += throughput * source * (1.0 - stepT) / extinction;
    throughput *= stepT;
  }
  if (toGround >= 0.0) {
    float hereKm = sqrt(radiusKm * radiusKm + toGround * toGround +
                        2.0 * radiusKm * toGround * cosView);
    float sunDot = dot(dir, sun);
    float cosSunAt = (radiusKm * cosSunZenith + toGround * sunDot) / hereKm;
    if (cosSunAt > 0.0) {
      vec3 toSunGround =
          textureLod(transmittance, mediumTransmittanceUv(medium, MediumLook(hereKm, cosSunAt)), 0.0)
              .rgb;
      summed += throughput * toSunGround * cosSunAt * vec3(medium.GroundAlbedo) /
                OUTSHINE_PI;
    }
  }
  return summed;
}

vec3
mediumTransmittance(Medium medium, MediumLook look, uint steps, float segment) {
  float radiusKm = look.RadiusKm;
  float cosZenith = look.CosZenith;
  float toGround = mediumGroundReach(medium, radiusKm, cosZenith);
  float toTop = mediumTopReach(medium, radiusKm, cosZenith);
  float span = toGround < 0.0 ? toTop : min(toTop, toGround);
  vec3 depth = vec3(0.0);
  float stride = span / float(steps);
  for (uint step = 0u; step < steps; step = step + 1u) {
    float along = stride * (float(step) + segment);
    depth += mediumExtinctionPerKm(medium, mediumHeightAlong(medium, radiusKm, cosZenith, along)) *
             stride;
  }
  return exp(-depth);
}

#endif
