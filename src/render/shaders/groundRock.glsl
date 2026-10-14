#include "rockCrevice.glsl"

vec4 rockFiltered(vec3 worldM, float footprintM, float wavelengthM) {
  if (wavelengthM <= 0.0) { return vec4(0.0); }
  float visibility = 1.0 - smoothstep(0.25, 0.8, footprintM / wavelengthM);
  if (visibility <= 0.0) { return vec4(0.0); }
  vec4 field = surfaceNoise3(worldM / wavelengthM);
  return vec4(field.xyz / wavelengthM, field.w - 0.5) * visibility;
}

GroundDetail rockDetail(vec3 worldM, vec3 worldDx, vec3 worldDy, float footprintM, uint rows) {
  uint rock = min(floatBitsToUint(groundPalette[1]), rows);
  uint at = 4u * (rows + 2u) + rows + 1u + rock * 8u;
  float heightAmplitudeM = groundPalette[at + 1u];
  float coarseM = groundPalette[at + 2u];
  float fineM = groundPalette[at + 3u];
  float contrast = clamp(groundPalette[at + 4u], 0.0, 1.0);
  if (coarseM <= 0.0 || fineM <= 0.0) { return GroundDetail(1.0, 0.0, vec2(0.0)); }
  float macroM = coarseM * 12.0;
  if (footprintM >= max(macroM, fineM) * 0.8) {
    return GroundDetail(1.0, 0.0, vec2(0.0));
  }
  float macroVisibility = 1.0 - smoothstep(0.25, 0.8, footprintM / macroM);
  float coarseVisibility = 1.0 - smoothstep(0.25, 0.8, footprintM * 1.04 / coarseM);
  vec4 macroField = macroVisibility > 0.0 ? surfaceNoise3(worldM / macroM) : vec4(0.0, 0.0, 0.0, 0.5);
  vec3 beddingM = vec3(dot(worldM, vec3(0.12, 1.0, 0.08)),
                       worldM.x * 0.125, worldM.z * 0.125);
  vec3 fracturesM = vec3(dot(worldM, vec3(0.94, 0.04, 0.34)),
                         worldM.y / 6.0,
                         dot(worldM, vec3(-0.34, 0.0, 0.94)) / 6.0);
  vec4 coarseField = coarseVisibility > 0.0 ? surfaceNoise3(beddingM / coarseM) : vec4(0.0, 0.0, 0.0, 0.5);
  vec4 fineField = rockFiltered(fracturesM, footprintM * 1.04, fineM);
  float macro = (macroField.w - 0.5) * macroVisibility;
  float coarse = (coarseField.w - 0.5) * coarseVisibility;
  float fine = fineField.w;
  float albedoScale = clamp(1.0 + contrast * (0.4 * macro + 0.5 * coarse + 0.1 * fine),
                            0.72, 1.28);
  float roughnessOffset = 0.14 * macro + 0.06 * coarse;
  float macroHeight = min(macroM * 0.025, 3.0);
  float coarseHeight = min(coarseM * 0.04, 0.4);
  vec2 macroRelief = rockCrevice(macroField.w - 0.5, footprintM, macroM);
  vec2 coarseRelief = rockCrevice(coarseField.w - 0.5, footprintM * 1.04, coarseM);
  vec3 coarseGradient = (coarseField.x * vec3(0.12, 1.0, 0.08) +
                        vec3(coarseField.y * 0.125, 0.0, coarseField.z * 0.125)) / coarseM;
  vec3 fineGradient = fineField.x * vec3(0.94, 0.04, 0.34) +
                     vec3(0.0, fineField.y / 6.0, 0.0) +
                     fineField.z * vec3(-0.34, 0.0, 0.94) / 6.0;
  vec3 gradient = macroHeight * macroRelief.y * macroField.xyz / macroM +
                  coarseHeight * coarseRelief.y * coarseGradient +
                  heightAmplitudeM * fineGradient;
  return GroundDetail(albedoScale, roughnessOffset,
                       vec2(dot(gradient, worldDx), dot(gradient, worldDy)));
}
