#ifndef OUTSHINE_RENDER_STAGES_MEDIUMCORE_H
#define OUTSHINE_RENDER_STAGES_MEDIUMCORE_H

static inline float mediumTopReach(MEDIUM_CONST Medium &medium, float radiusKm, float cosZenith) {
  float under =
      radiusKm * radiusKm * (cosZenith * cosZenith - 1.0) + medium.TopRadiusKm * medium.TopRadiusKm;
  if (under < 0.0) { return 0.0; }
  return max(0.0, -radiusKm * cosZenith + sqrt(under));
}

static inline float
mediumGroundReach(MEDIUM_CONST Medium &medium, float radiusKm, float cosZenith) {
  float under = radiusKm * radiusKm * (cosZenith * cosZenith - 1.0) +
                medium.BottomRadiusKm * medium.BottomRadiusKm;
  if (under < 0.0) { return -1.0; }
  float entry = -radiusKm * cosZenith - sqrt(under);
  return entry >= 0.0 ? entry : -1.0;
}

static inline float
mediumHeightAlong(MEDIUM_CONST Medium &medium, float radiusKm, float cosZenith, float alongKm) {
  float above = radiusKm - medium.BottomRadiusKm;
  float raised = above * (radiusKm + medium.BottomRadiusKm) + alongKm * alongKm +
                 2.0 * radiusKm * alongKm * cosZenith;
  float sampleKm =
      sqrt(radiusKm * radiusKm + alongKm * alongKm + 2.0 * radiusKm * alongKm * cosZenith);
  return raised / (sampleKm + medium.BottomRadiusKm);
}

static inline void mediumTransmittanceParams(MEDIUM_CONST Medium &medium,
                                             float u,
                                             float v,
                                             MEDIUM_THREAD float &radiusKm,
                                             MEDIUM_THREAD float &cosZenith) {
  float span = sqrt(
      max(0.0,
          medium.TopRadiusKm * medium.TopRadiusKm - medium.BottomRadiusKm * medium.BottomRadiusKm));
  float ground = span * v;
  radiusKm = sqrt(ground * ground + medium.BottomRadiusKm * medium.BottomRadiusKm);
  float shortest = medium.TopRadiusKm - radiusKm;
  float longest = ground + span;
  float reach = shortest + u * (longest - shortest);
  cosZenith = reach == 0.0
                  ? 1.0
                  : (span * span - ground * ground - reach * reach) / (2.0 * radiusKm * reach);
  cosZenith = clamp(cosZenith, -1.0, 1.0);
}

static inline float rayleighPhase(float cosTheta) {
  return 3.0 / (16.0 * OUTSHINE_PI) * (1.0 + cosTheta * cosTheta);
}

static inline float miePhase(float g, float cosTheta) {
  float k = 3.0 / (8.0 * OUTSHINE_PI) * (1.0 - g * g) / (2.0 + g * g);
  return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5);
}

static inline float subUvsToUnit(float u, float resolution) {
  return (u - 0.5 / resolution) * (resolution / (resolution - 1.0));
}

static inline float unitToSubUvs(float u, float resolution) {
  return (u + 0.5 / resolution) * (resolution / (resolution + 1.0));
}

static inline void skyViewParams(MEDIUM_CONST Medium &medium,
                                 float radiusKm,
                                 float unitU,
                                 float unitV,
                                 float widthPx,
                                 float heightPx,
                                 MEDIUM_THREAD float &cosView,
                                 MEDIUM_THREAD float &lightViewCos) {
  float u = subUvsToUnit(unitU, widthPx);
  float v = subUvsToUnit(unitV, heightPx);
  float toHorizon =
      sqrt(max(0.0, radiusKm * radiusKm - medium.BottomRadiusKm * medium.BottomRadiusKm));
  float beta = acos(clamp(toHorizon / radiusKm, -1.0, 1.0));
  float zenithToHorizon = OUTSHINE_PI - beta;
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

#endif
