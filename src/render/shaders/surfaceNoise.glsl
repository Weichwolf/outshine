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

vec4 surfaceNoise3(vec3 at) {
  ivec3 cell = ivec3(floor(at));
  vec3 f = fract(at);
  vec3 slope = 6.0 * f * (1.0 - f);
  f = f * f * (3.0 - 2.0 * f);
  float a0 = surfaceValue(cell);
  float a1 = surfaceValue(cell + ivec3(1, 0, 0));
  float b0 = surfaceValue(cell + ivec3(0, 1, 0));
  float b1 = surfaceValue(cell + ivec3(1, 1, 0));
  float c0 = surfaceValue(cell + ivec3(0, 0, 1));
  float c1 = surfaceValue(cell + ivec3(1, 0, 1));
  float d0 = surfaceValue(cell + ivec3(0, 1, 1));
  float d1 = surfaceValue(cell + ivec3(1, 1, 1));
  float a = mix(a0, a1, f.x);
  float b = mix(b0, b1, f.x);
  float c = mix(c0, c1, f.x);
  float d = mix(d0, d1, f.x);
  float low = mix(a, b, f.y);
  float high = mix(c, d, f.y);
  return vec4(mix(mix(a1 - a0, b1 - b0, f.y), mix(c1 - c0, d1 - d0, f.y), f.z) * slope.x,
              mix(b - a, d - c, f.z) * slope.y,
              (high - low) * slope.z, mix(low, high, f.z));
}

vec3 surfaceNoise2(vec2 at) {
  ivec2 cell = ivec2(floor(at));
  vec2 f = fract(at);
  vec2 slope = 6.0 * f * (1.0 - f);
  f = f * f * (3.0 - 2.0 * f);
  float a0 = surfaceValue(ivec3(cell, 0));
  float a1 = surfaceValue(ivec3(cell + ivec2(1, 0), 0));
  float b0 = surfaceValue(ivec3(cell + ivec2(0, 1), 0));
  float b1 = surfaceValue(ivec3(cell + ivec2(1, 1), 0));
  float a = mix(a0, a1, f.x);
  float b = mix(b0, b1, f.x);
  return vec3(mix(a1 - a0, b1 - b0, f.y) * slope.x,
              (b - a) * slope.y, mix(a, b, f.y));
}
