#include "rockCrevice.glsl"

vec3 rockBedding(vec3 at) {
  return vec3(dot(at, vec3(0.12, 1.0, 0.08)), at.x * 0.125, at.z * 0.125);
}

vec3 rockFractures(vec3 at) {
  return vec3(dot(at, vec3(0.94, 0.04, 0.34)), at.y / 6.0,
               dot(at, vec3(-0.34, 0.0, 0.94)) / 6.0);
}

vec4 rockFiltered(vec3 at, vec3 dx, vec3 dy, float wavelengthM, bool crevice) {
  if (wavelengthM <= 0.0) { return vec4(0.0); }
  vec3 tx = dx / wavelengthM;
  vec3 ty = dy / wavelengthM;
  float squareSize = max(dot(tx, tx), dot(ty, ty));
  if (squareSize >= 0.64) { return vec4(0.0); }
  float visibility = squareSize <= 0.0625 ? 1.0 : 1.0 - smoothstep(0.25, 0.8, sqrt(squareSize));
  vec4 field = surfaceNoise3(at / wavelengthM);
  field.w -= 0.5;
  vec3 gradient = field.xyz / wavelengthM;
  if (crevice) {
    vec2 phase = vec2(dot(field.xyz, tx), dot(field.xyz, ty));
    gradient *= rockCrevice(field.w, dot(phase, phase) / 12.0).y;
  }
  return vec4(gradient, field.w) * visibility;
}

GroundDetail rockDetail(vec3 worldM, vec3 worldDx, vec3 worldDy, uint rows) {
  uint rock = min(floatBitsToUint(groundPalette[1]), rows);
  uint at = 4u * (rows + 2u) + rows + 1u + rock * 8u;
  float heightAmplitudeM = groundPalette[at + 1u];
  float coarseM = groundPalette[at + 2u];
  float fineM = groundPalette[at + 3u];
  float contrast = clamp(groundPalette[at + 4u], 0.0, 1.0);
  if (coarseM <= 0.0 || fineM <= 0.0) { return GroundDetail(1.0, 0.0, vec2(0.0)); }
  float macroM = coarseM * 12.0;
  vec4 macro = rockFiltered(worldM, worldDx, worldDy, macroM, true);
  vec4 coarse = rockFiltered(rockBedding(worldM), rockBedding(worldDx), rockBedding(worldDy), coarseM, true);
  vec4 fine = rockFiltered(rockFractures(worldM), rockFractures(worldDx), rockFractures(worldDy), fineM, false);
  float albedoScale = clamp(1.0 + contrast * (0.4 * macro.w + 0.5 * coarse.w + 0.1 * fine.w), 0.72, 1.28);
  float roughnessOffset = 0.14 * macro.w + 0.06 * coarse.w;
  float macroHeight = min(macroM * 0.025, 3.0);
  float coarseHeight = min(coarseM * 0.04, 0.4);
  vec3 coarseGradient = coarse.x * vec3(0.12, 1.0, 0.08) + vec3(coarse.y * 0.125, 0.0, coarse.z * 0.125);
  vec3 fineGradient = fine.x * vec3(0.94, 0.04, 0.34) + vec3(0.0, fine.y / 6.0, 0.0) +
                      fine.z * vec3(-0.34, 0.0, 0.94) / 6.0;
  vec3 gradient = macroHeight * macro.xyz + coarseHeight * coarseGradient + heightAmplitudeM * fineGradient;
  return GroundDetail(albedoScale, roughnessOffset, vec2(dot(gradient, worldDx), dot(gradient, worldDy)));
}
