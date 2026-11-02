#ifndef OUTSHINE_SURFACE_HASH_GLSL
#define OUTSHINE_SURFACE_HASH_GLSL
uint surfaceHash(ivec3 cell) {
  uvec3 p = uvec3(cell);
  uint h = p.x * 0x9e3779b9u ^ p.y * 0x85ebca6bu ^ p.z * 0xc2b2ae35u;
  h ^= h >> 16u;
  h *= 0x7feb352du;
  h ^= h >> 15u;
  h *= 0x846ca68bu;
  return h ^ (h >> 16u);
}
#endif
