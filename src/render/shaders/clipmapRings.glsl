struct ClipmapRing {
#define CLIPMAP_FLOAT4(name) vec4 name;
#define CLIPMAP_INT4(name) ivec4 name;
#include "render/terrain/ClipmapRing.inc"
#undef CLIPMAP_FLOAT4
#undef CLIPMAP_INT4
};
layout(std430, set = 0, binding = 1) readonly buffer ClipmapRings { ClipmapRing clipmapRings[]; };

float clipmapMorph(vec2 grid, vec4 morph) {
  float distance = max(abs(grid.x - morph.x), abs(grid.y - morph.y));
  return clamp((distance - (morph.z - morph.w)) / morph.w, 0.0, 1.0);
}
