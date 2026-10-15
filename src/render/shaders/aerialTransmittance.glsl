#ifndef OUTSHINE_AERIAL_TRANSMITTANCE_GLSL
#define OUTSHINE_AERIAL_TRANSMITTANCE_GLSL

#include "medium.glsl"

vec3 aerialTransmittance(Medium air, sampler2D veil, float radiusKm,
                         float cosZenith, float reachKm) {
  float endRadiusKm = sqrt(max(air.BottomRadiusKm * air.BottomRadiusKm,
      radiusKm * radiusKm + reachKm * reachKm + 2.0 * radiusKm * reachKm * cosZenith));
  float endCosZenith = clamp((radiusKm * cosZenith + reachKm) / endRadiusKm, -1.0, 1.0);
  if (mediumGroundReach(air, radiusKm, cosZenith) < 0.0) {
    vec3 atEye = textureLod(veil, mediumTransmittanceUv(air, MediumLook(radiusKm, cosZenith)), 0.0).rgb;
    vec3 atEnd = textureLod(veil, mediumTransmittanceUv(air, MediumLook(endRadiusKm, endCosZenith)), 0.0).rgb;
    return clamp(atEye / max(atEnd, vec3(1.0e-6)), vec3(0.0), vec3(1.0));
  }
  vec3 atEye = textureLod(veil, mediumTransmittanceUv(air, MediumLook(radiusKm, -cosZenith)), 0.0).rgb;
  vec3 atEnd = textureLod(veil, mediumTransmittanceUv(air, MediumLook(endRadiusKm, -endCosZenith)), 0.0).rgb;
  return clamp(atEnd / max(atEye, vec3(1.0e-6)), vec3(0.0), vec3(1.0));
}

#endif
