struct RockDetail {
  float AlbedoScale;
  float RoughnessOffset;
  float BumpM;
};

uint rockHash(ivec3 cell) {
  uvec3 p = uvec3(cell);
  uint h = p.x * 0x9e3779b9u ^ p.y * 0x85ebca6bu ^ p.z * 0xc2b2ae35u;
  h ^= h >> 16u;
  h *= 0x7feb352du;
  h ^= h >> 15u;
  h *= 0x846ca68bu;
  return h ^ (h >> 16u);
}

float rockValue(ivec3 cell) {
  return float(rockHash(cell) & 0x00ffffffu) * (1.0 / 16777215.0);
}

float rockNoise(vec3 at) {
  ivec3 cell = ivec3(floor(at));
  vec3 f = fract(at);
  f = f * f * (3.0 - 2.0 * f);
  float a = mix(rockValue(cell), rockValue(cell + ivec3(1, 0, 0)), f.x);
  float b = mix(rockValue(cell + ivec3(0, 1, 0)),
                rockValue(cell + ivec3(1, 1, 0)), f.x);
  float c = mix(rockValue(cell + ivec3(0, 0, 1)),
                rockValue(cell + ivec3(1, 0, 1)), f.x);
  float d = mix(rockValue(cell + ivec3(0, 1, 1)),
                rockValue(cell + ivec3(1, 1, 1)), f.x);
  return mix(mix(a, b, f.y), mix(c, d, f.y), f.z);
}

float rockFiltered(vec3 worldM, float footprintM, float wavelengthM) {
  if (wavelengthM <= 0.0) { return 0.5; }
  float visibility = 1.0 - smoothstep(0.25, 0.8, footprintM / wavelengthM);
  return visibility > 0.0
      ? mix(0.5, rockNoise(worldM / wavelengthM), visibility)
      : 0.5;
}

RockDetail rockDetail(vec3 worldM, float footprintM, uint rows) {
  uint rock = min(floatBitsToUint(groundPalette[1]), rows);
  uint at = 4u * (rows + 2u) + rows + 1u + rock * 8u;
  float heightAmplitudeM = groundPalette[at + 1u];
  float coarseM = groundPalette[at + 2u];
  float fineM = groundPalette[at + 3u];
  float contrast = clamp(groundPalette[at + 4u], 0.0, 1.0);
  if (coarseM <= 0.0 || fineM <= 0.0) { return RockDetail(1.0, 0.0, 0.0); }
  float macroM = coarseM * 12.0;
  float macro = rockFiltered(worldM, footprintM, macroM) - 0.5;
  float coarse = rockFiltered(worldM, footprintM, coarseM) - 0.5;
  float fine = rockFiltered(worldM, footprintM, fineM) - 0.5;
  float albedoScale = clamp(1.0 + contrast * (0.65 * macro + 0.25 * coarse + 0.1 * fine),
                            0.72, 1.28);
  float roughnessOffset = 0.14 * macro + 0.06 * coarse;
  float bumpM = min(macroM * 0.025, 3.0) * macro +
                min(coarseM * 0.04, 0.4) * coarse + heightAmplitudeM * fine;
  return RockDetail(albedoScale, roughnessOffset, bumpM);
}

vec3 rockBumpNormal(vec3 n, vec3 positionM, float heightM) {
  vec3 q0 = dFdx(positionM);
  vec3 q1 = dFdy(positionM);
  vec3 r0 = cross(q1, n);
  vec3 r1 = cross(n, q0);
  float det = dot(q0, r0);
  if (abs(det) < 1.0e-8) { return n; }
  vec3 gradient = sign(det) * (dFdx(heightM) * r0 + dFdy(heightM) * r1) / abs(det);
  return normalize(n - gradient);
}
