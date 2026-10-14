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
  if (coarseM <= 0.0 || fineM <= 0.0) { return GroundDetail(1.0, 0.0, 0.0, vec2(0.0)); }
  float macroM = coarseM * 12.0;
  vec4 macroField = rockFiltered(worldM, footprintM, macroM);
  vec3 beddingM = vec3(dot(worldM, vec3(0.12, 1.0, 0.08)),
                       worldM.x * 0.125, worldM.z * 0.125);
  vec3 fracturesM = vec3(dot(worldM, vec3(0.94, 0.04, 0.34)),
                         worldM.y / 6.0,
                         dot(worldM, vec3(-0.34, 0.0, 0.94)) / 6.0);
  vec4 coarseField = rockFiltered(beddingM, footprintM * 1.04, coarseM);
  vec4 fineField = rockFiltered(fracturesM, footprintM * 1.04, fineM);
  float macro = macroField.w;
  float coarse = coarseField.w;
  float fine = fineField.w;
  float albedoScale = clamp(1.0 + contrast * (0.4 * macro + 0.5 * coarse + 0.1 * fine),
                            0.72, 1.28);
  float roughnessOffset = 0.14 * macro + 0.06 * coarse;
  float macroHeight = min(macroM * 0.025, 3.0);
  float coarseHeight = min(coarseM * 0.04, 0.4);
  float bumpM = macroHeight * macro + coarseHeight * coarse + heightAmplitudeM * fine;
  vec3 coarseGradient = coarseField.x * vec3(0.12, 1.0, 0.08) +
                        vec3(coarseField.y * 0.125, 0.0, coarseField.z * 0.125);
  vec3 fineGradient = fineField.x * vec3(0.94, 0.04, 0.34) +
                     vec3(0.0, fineField.y / 6.0, 0.0) +
                     fineField.z * vec3(-0.34, 0.0, 0.94) / 6.0;
  vec3 gradient = macroHeight * macroField.xyz + coarseHeight * coarseGradient +
                  heightAmplitudeM * fineGradient;
  return GroundDetail(albedoScale, roughnessOffset, bumpM,
                       vec2(dot(gradient, worldDx), dot(gradient, worldDy)));
}
