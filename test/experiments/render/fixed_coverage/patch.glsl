layout(std140, set = 1, binding = 0) uniform Patch {
  ivec4 corners[8];
  uvec4 meta;
  vec4 depth;
} fixedPatch;

bool validPatch() {
  if (fixedPatch.meta.x < 3u || fixedPatch.meta.x > 8u ||
      fixedPatch.meta.y == 0u || fixedPatch.meta.z == 0u ||
      fixedPatch.meta.y > 16384u || fixedPatch.meta.z > 16384u ||
      fixedPatch.meta.w == 0u || fixedPatch.meta.w > 8u ||
      fixedPatch.meta.y * fixedPatch.meta.w > 16384u || fixedPatch.meta.z * fixedPatch.meta.w > 16384u ||
      isnan(fixedPatch.depth.x) || isinf(fixedPatch.depth.x) ||
      !(fixedPatch.depth.x > 0.0 && fixedPatch.depth.x <= 1.0)) { return false; }
  for (uint i = 0u; i < fixedPatch.meta.x; ++i) {
    ivec2 p = fixedPatch.corners[i].xy;
    if (any(lessThan(p, ivec2(0))) ||
        any(greaterThan(p, ivec2(fixedPatch.meta.yz * fixedPatch.meta.w)))) { return false; }
  }
  return true;
}
