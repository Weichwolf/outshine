uint surfaceHash(ivec3 cell) {
  uvec3 p = uvec3(cell);
  uint h = p.x * 0x9e3779b9u ^ p.y * 0x85ebca6bu ^ p.z * 0xc2b2ae35u;
  h ^= h >> 16u;
  h *= 0x7feb352du;
  h ^= h >> 15u;
  h *= 0x846ca68bu;
  return h ^ (h >> 16u);
}

float surfaceValue(ivec3 cell) {
  return float(surfaceHash(cell) & 0x00ffffffu) * (1.0 / 16777215.0);
}

float surfaceNoise3(vec3 at) {
  ivec3 cell = ivec3(floor(at));
  vec3 f = fract(at);
  f = f * f * (3.0 - 2.0 * f);
  float a = mix(surfaceValue(cell), surfaceValue(cell + ivec3(1, 0, 0)), f.x);
  float b = mix(surfaceValue(cell + ivec3(0, 1, 0)),
                surfaceValue(cell + ivec3(1, 1, 0)), f.x);
  float c = mix(surfaceValue(cell + ivec3(0, 0, 1)),
                surfaceValue(cell + ivec3(1, 0, 1)), f.x);
  float d = mix(surfaceValue(cell + ivec3(0, 1, 1)),
                surfaceValue(cell + ivec3(1, 1, 1)), f.x);
  return mix(mix(a, b, f.y), mix(c, d, f.y), f.z);
}


float surfaceNoise2(vec2 at) {
  ivec2 cell = ivec2(floor(at));
  vec2 f = fract(at);
  f = f * f * (3.0 - 2.0 * f);
  float a = mix(surfaceValue(ivec3(cell, 0)), surfaceValue(ivec3(cell + ivec2(1, 0), 0)), f.x);
  float b = mix(surfaceValue(ivec3(cell + ivec2(0, 1), 0)),
                surfaceValue(ivec3(cell + ivec2(1, 1), 0)), f.x);
  return mix(a, b, f.y);
}
