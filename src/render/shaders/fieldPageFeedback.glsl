#ifndef OUTSHINE_FIELD_PAGE_FEEDBACK_GLSL
#define OUTSHINE_FIELD_PAGE_FEEDBACK_GLSL
vec4 fieldPageFeedback(ivec3 address) {
  if (address.z < 0 || address.z > 31) { return vec4(0.0, 0.0, -1.0, 0.0); }
  uvec2 tile = uvec2(address.xy);
  uint high = (tile.x >> 24u) | ((tile.y >> 24u) << 8u) | (uint(address.z) << 16u) | (1u << 21u);
  return vec4(vec2(tile & 0x00ffffffu), float(high), 0.0);
}
#endif
