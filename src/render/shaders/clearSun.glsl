#ifndef OUTSHINE_CLEAR_SUN_GLSL
#define OUTSHINE_CLEAR_SUN_GLSL
#include "medium.glsl"
#define MEDIUM_ARG Medium
#define MEDIUM_INLINE
#define SUN_NOEXCEPT
#include "world/sky/ClearSunCore.h"
#undef SUN_NOEXCEPT
#undef MEDIUM_INLINE
#undef MEDIUM_ARG

vec3 clearSunUnoccludedTransfer(Medium medium, MediumLook point, sampler2D veil) {
  if (point.RadiusKm > medium.TopRadiusKm) {
    float projection = point.RadiusKm * point.CosZenith;
    float discriminant = projection * projection +
        (medium.TopRadiusKm - point.RadiusKm) * (medium.TopRadiusKm + point.RadiusKm);
    if (point.CosZenith >= 0.0 || discriminant <= 0.0) { return vec3(1.0); }
    point.RadiusKm = medium.TopRadiusKm;
    point.CosZenith = -sqrt(discriminant) / medium.TopRadiusKm;
  }
  vec3 value = textureLod(veil, mediumTransmittanceUv(medium, point, textureSize(veil, 0)), 0.0).rgb;
  return value;
}

vec3 clearSunPhysicalTransfer(Medium medium, MediumLook point, sampler2D veil) {
  if (!mediumPhysicalSunVisible(medium, point)) { return vec3(0.0); }
  return clearSunUnoccludedTransfer(medium, point, veil);
}

vec3 clearSunLegacyTransfer(Medium medium, MediumLook point, sampler2D veil) {
  if (!mediumSunVisible(medium, point, kMediumGroundLiftKm)) { return vec3(0.0); }
  return clearSunUnoccludedTransfer(medium, point, veil);
}

#endif
