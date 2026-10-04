#ifndef OUTSHINE_WORLD_SKY_ATMOSPHERECORE_H
#define OUTSHINE_WORLD_SKY_ATMOSPHERECORE_H

#define OUTSHINE_RAYLEIGH_NUMERATOR 3.0f
#define OUTSHINE_RAYLEIGH_DENOMINATOR 16.0f
#define OUTSHINE_MIE_EXPONENT 1.5f

MEDIUM_INLINE float mediumTopReach(MEDIUM_ARG medium, float radiusKm, float cosZenith) {
  const float under = radiusKm * radiusKm * (cosZenith * cosZenith - 1.0f) +
                      medium.TopRadiusKm * medium.TopRadiusKm;
  if (under < 0.0f) { return 0.0f; }
  return max(0.0f, -radiusKm * cosZenith + sqrt(under));
}

MEDIUM_INLINE float mediumGroundReach(MEDIUM_ARG medium, float radiusKm, float cosZenith) {
  const float under = radiusKm * radiusKm * (cosZenith * cosZenith - 1.0f) +
                      medium.BottomRadiusKm * medium.BottomRadiusKm;
  if (under < 0.0f) { return -1.0f; }
  const float entry = -radiusKm * cosZenith - sqrt(under);
  return entry >= 0.0f ? entry : -1.0f;
}

MEDIUM_INLINE float
mediumHeightAlong(MEDIUM_ARG medium, float radiusKm, float cosZenith, float alongKm) {
  const float above = radiusKm - medium.BottomRadiusKm;
  const float raised = above * (radiusKm + medium.BottomRadiusKm) + alongKm * alongKm +
                       2.0f * radiusKm * alongKm * cosZenith;
  const float sampleKm =
      sqrt(radiusKm * radiusKm + alongKm * alongKm + 2.0f * radiusKm * alongKm * cosZenith);
  return raised / (sampleKm + medium.BottomRadiusKm);
}

MEDIUM_INLINE MediumLook mediumTransmittanceParams(MEDIUM_ARG medium, MediumUv uv) {
  const float span = sqrt(
      max(0.0f,
          medium.TopRadiusKm * medium.TopRadiusKm - medium.BottomRadiusKm * medium.BottomRadiusKm));
  const float ground = span * uv.V;
  const float radiusKm = sqrt(ground * ground + medium.BottomRadiusKm * medium.BottomRadiusKm);
  const float shortest = medium.TopRadiusKm - radiusKm;
  const float longest = ground + span;
  const float reach = shortest + uv.U * (longest - shortest);
  const float cosZenith =
      reach == 0.0f ? 1.0f
                    : (span * span - ground * ground - reach * reach) / (2.0f * radiusKm * reach);
  MediumLook look;
  look.RadiusKm = radiusKm;
  look.CosZenith = clamp(cosZenith, -1.0f, 1.0f);
  return look;
}

MEDIUM_INLINE float rayleighPhase(float cosTheta) {
  return OUTSHINE_RAYLEIGH_NUMERATOR / (OUTSHINE_RAYLEIGH_DENOMINATOR * OUTSHINE_PI) *
         (1.0f + cosTheta * cosTheta);
}

MEDIUM_INLINE float miePhase(float g, float cosTheta) {
  const float k = 3.0f / (8.0f * OUTSHINE_PI) * (1.0f - g * g) / (2.0f + g * g);
  return k * (1.0f + cosTheta * cosTheta) /
         pow(1.0f + g * g - 2.0f * g * cosTheta, OUTSHINE_MIE_EXPONENT);
}

MEDIUM_INLINE float subUvsToUnit(float u, float resolution) {
  return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f));
}

MEDIUM_INLINE float unitToSubUvs(float u, float resolution) {
  return (u * (resolution - 1.0f) + 0.5f) / resolution;
}

MEDIUM_INLINE MediumLook multiScatterParams(MEDIUM_ARG medium,
                                            MediumUv sub,
                                            MediumLutSize size,
                                            float liftKm) {
  MediumLook look;
  look.RadiusKm =
      medium.BottomRadiusKm + liftKm +
      subUvsToUnit(sub.V, size.HeightPx) * (medium.TopRadiusKm - medium.BottomRadiusKm - liftKm);
  look.CosZenith = subUvsToUnit(sub.U, size.WidthPx) * 2.0f - 1.0f;
  return look;
}

MEDIUM_INLINE MediumUv multiScatterSample(MEDIUM_ARG medium,
                                          MediumLook look,
                                          MediumLutSize size,
                                          float liftKm) {
  MediumUv uv;
  uv.U = unitToSubUvs(look.CosZenith * 0.5f + 0.5f, size.WidthPx);
  uv.V = unitToSubUvs((look.RadiusKm - medium.BottomRadiusKm - liftKm) /
                          (medium.TopRadiusKm - medium.BottomRadiusKm - liftKm),
                      size.HeightPx);
  return uv;
}

MEDIUM_INLINE SkyViewLook skyViewParams(MEDIUM_ARG medium,
                                        float radiusKm,
                                        MediumUv sub,
                                        MediumLutSize size) {
  const float u = subUvsToUnit(sub.U, size.WidthPx);
  const float v = subUvsToUnit(sub.V, size.HeightPx);
  const float toHorizon =
      sqrt(max(0.0f, radiusKm * radiusKm - medium.BottomRadiusKm * medium.BottomRadiusKm));
  const float beta = acos(clamp(toHorizon / radiusKm, -1.0f, 1.0f));
  const float pi = OUTSHINE_PI;
  const float zenithToHorizon = pi - beta;
  float cosView = 0.0f;
  if (v < 0.5f) {
    float coord = 1.0f - 2.0f * v;
    coord = 1.0f - coord * coord;
    cosView = cos(zenithToHorizon * coord);
  } else {
    float coord = v * 2.0f - 1.0f;
    coord *= coord;
    cosView = cos(zenithToHorizon + beta * coord);
  }
  SkyViewLook look;
  look.CosView = cosView;
  look.LightViewCos = -(u * u * 2.0f - 1.0f);
  return look;
}

MEDIUM_INLINE SkyViewSample skyViewSample(float bottomRadiusKm,
                                          float radiusKm,
                                          SkyViewLook look,
                                          MediumLutSize size) {
  const float toHorizon = sqrt(max(0.0f, radiusKm * radiusKm - bottomRadiusKm * bottomRadiusKm));
  const float beta = acos(clamp(toHorizon / radiusKm, -1.0f, 1.0f));
  const float pi = OUTSHINE_PI;
  const float zenithToHorizon = pi - beta;
  const float zenith = acos(clamp(look.CosView, -1.0f, 1.0f));
  const bool hitsGround = zenith > zenithToHorizon;
  float v = 0.0f;
  if (!hitsGround) {
    const float coord = zenith / zenithToHorizon;
    v = (1.0f - sqrt(max(0.0f, 1.0f - coord))) * 0.5f;
  } else {
    const float coord = (zenith - zenithToHorizon) / beta;
    v = sqrt(max(0.0f, coord)) * 0.5f + 0.5f;
  }
  const float u = sqrt(max(0.0f, -look.LightViewCos * 0.5f + 0.5f));
  SkyViewSample sampled;
  sampled.Uv.U = unitToSubUvs(u, size.WidthPx);
  sampled.Uv.V = unitToSubUvs(v, size.HeightPx);
  sampled.HitsGround = hitsGround;
  return sampled;
}

#endif
