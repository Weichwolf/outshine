#ifndef OUTSHINE_RENDER_STAGES_MEDIUMCORE_H
#define OUTSHINE_RENDER_STAGES_MEDIUMCORE_H

#define OUTSHINE_RAYLEIGH_NUMERATOR 3.0f
#define OUTSHINE_RAYLEIGH_DENOMINATOR 16.0f
#define OUTSHINE_MIE_EXPONENT 1.5f

static inline float mediumTopReach(MEDIUM_CONST Medium &medium, float radiusKm, float cosZenith) {
  const float under = radiusKm * radiusKm * (cosZenith * cosZenith - 1.0f) +
                      medium.TopRadiusKm * medium.TopRadiusKm;
  if (under < 0.0f) { return 0.0f; }
  return max(0.0f, -radiusKm * cosZenith + sqrt(under));
}

static inline float
mediumGroundReach(MEDIUM_CONST Medium &medium, float radiusKm, float cosZenith) {
  const float under = radiusKm * radiusKm * (cosZenith * cosZenith - 1.0f) +
                      medium.BottomRadiusKm * medium.BottomRadiusKm;
  if (under < 0.0f) { return -1.0f; }
  const float entry = -radiusKm * cosZenith - sqrt(under);
  return entry >= 0.0f ? entry : -1.0f;
}

static inline float
mediumHeightAlong(MEDIUM_CONST Medium &medium, float radiusKm, float cosZenith, float alongKm) {
  const float above = radiusKm - medium.BottomRadiusKm;
  const float raised = above * (radiusKm + medium.BottomRadiusKm) + alongKm * alongKm +
                       2.0f * radiusKm * alongKm * cosZenith;
  const float sampleKm =
      sqrt(radiusKm * radiusKm + alongKm * alongKm + 2.0f * radiusKm * alongKm * cosZenith);
  return raised / (sampleKm + medium.BottomRadiusKm);
}

static inline void mediumTransmittanceParams(MEDIUM_CONST Medium &medium,
                                             float u,
                                             float v,
                                             MEDIUM_THREAD float &radiusKm,
                                             MEDIUM_THREAD float &cosZenith) {
  const float span = sqrt(
      max(0.0f,
          medium.TopRadiusKm * medium.TopRadiusKm - medium.BottomRadiusKm * medium.BottomRadiusKm));
  const float ground = span * v;
  radiusKm = sqrt(ground * ground + medium.BottomRadiusKm * medium.BottomRadiusKm);
  const float shortest = medium.TopRadiusKm - radiusKm;
  const float longest = ground + span;
  const float reach = shortest + u * (longest - shortest);
  cosZenith = reach == 0.0f
                  ? 1.0f
                  : (span * span - ground * ground - reach * reach) / (2.0f * radiusKm * reach);
  cosZenith = clamp(cosZenith, -1.0f, 1.0f);
}

static inline float rayleighPhase(float cosTheta) {
  return OUTSHINE_RAYLEIGH_NUMERATOR / (OUTSHINE_RAYLEIGH_DENOMINATOR * OUTSHINE_PI) *
         (1.0f + cosTheta * cosTheta);
}

static inline float miePhase(float g, float cosTheta) {
  const float k = 3.0f / (8.0f * OUTSHINE_PI) * (1.0f - g * g) / (2.0f + g * g);
  return k * (1.0f + cosTheta * cosTheta) /
         pow(1.0f + g * g - 2.0f * g * cosTheta, OUTSHINE_MIE_EXPONENT);
}

static inline float subUvsToUnit(float u, float resolution) {
  return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f));
}

static inline float unitToSubUvs(float u, float resolution) {
  return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f));
}

static inline void skyViewParams(MEDIUM_CONST Medium &medium,
                                 float radiusKm,
                                 float unitU,
                                 float unitV,
                                 float widthPx,
                                 float heightPx,
                                 MEDIUM_THREAD float &cosView,
                                 MEDIUM_THREAD float &lightViewCos) {
  const float u = subUvsToUnit(unitU, widthPx);
  const float v = subUvsToUnit(unitV, heightPx);
  const float toHorizon =
      sqrt(max(0.0f, radiusKm * radiusKm - medium.BottomRadiusKm * medium.BottomRadiusKm));
  const float beta = acos(clamp(toHorizon / radiusKm, -1.0f, 1.0f));
  const float zenithToHorizon = OUTSHINE_PI - beta;
  if (v < 0.5f) {
    float coord = 1.0f - 2.0f * v;
    coord = 1.0f - coord * coord;
    cosView = cos(zenithToHorizon * coord);
  } else {
    float coord = v * 2.0f - 1.0f;
    coord *= coord;
    cosView = cos(zenithToHorizon + beta * coord);
  }
  lightViewCos = -(u * u * 2.0f - 1.0f);
}

#endif
