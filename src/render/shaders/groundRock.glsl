float rockFiltered(vec3 worldM, float footprintM, float wavelengthM) {
  if (wavelengthM <= 0.0) { return 0.5; }
  float visibility = 1.0 - smoothstep(0.25, 0.8, footprintM / wavelengthM);
  return visibility > 0.0
      ? mix(0.5, surfaceNoise3(worldM / wavelengthM), visibility)
      : 0.5;
}

GroundDetail rockDetail(vec3 worldM, float footprintM, uint rows) {
  uint rock = min(floatBitsToUint(groundPalette[1]), rows);
  uint at = 4u * (rows + 2u) + rows + 1u + rock * 8u;
  float heightAmplitudeM = groundPalette[at + 1u];
  float coarseM = groundPalette[at + 2u];
  float fineM = groundPalette[at + 3u];
  float contrast = clamp(groundPalette[at + 4u], 0.0, 1.0);
  if (coarseM <= 0.0 || fineM <= 0.0) { return GroundDetail(1.0, 0.0, 0.0); }
  float macroM = coarseM * 12.0;
  float macro = rockFiltered(worldM, footprintM, macroM) - 0.5;
  vec3 beddingM = vec3(dot(worldM, vec3(0.12, 1.0, 0.08)),
                       worldM.x * 0.125, worldM.z * 0.125);
  vec3 fracturesM = vec3(dot(worldM, vec3(0.94, 0.04, 0.34)),
                         worldM.y / 6.0,
                         dot(worldM, vec3(-0.34, 0.0, 0.94)) / 6.0);
  float coarse = rockFiltered(beddingM, footprintM * 1.04, coarseM) - 0.5;
  float fine = rockFiltered(fracturesM, footprintM * 1.04, fineM) - 0.5;
  float albedoScale = clamp(1.0 + contrast * (0.4 * macro + 0.5 * coarse + 0.1 * fine),
                            0.72, 1.28);
  float roughnessOffset = 0.14 * macro + 0.06 * coarse;
  float bumpM = min(macroM * 0.025, 3.0) * macro +
                min(coarseM * 0.04, 0.4) * coarse + heightAmplitudeM * fine;
  return GroundDetail(albedoScale, roughnessOffset, bumpM);
}
