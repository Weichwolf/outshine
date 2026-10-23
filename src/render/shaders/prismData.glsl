#ifndef OUTSHINE_PRISM_DATA
#define OUTSHINE_PRISM_DATA
struct PrismPatch {
  mat4 current;
  mat4 previous;
  uvec4 samples;
  vec4 shape;
  vec4 bounds;
};
#if PRISM_VERTEX
layout(std430, set = 0, binding = 0) readonly buffer PrismPatches {
  PrismPatch prismPatches[];
};
#else
#if PRISM_DEPTH
#define PRISM_SAMPLE_BINDING 0
#define PRISM_PATCH_BINDING 1
#else
#define PRISM_SAMPLE_BINDING 3
#define PRISM_PATCH_BINDING 4
#endif
layout(std430, set = 4, binding = PRISM_SAMPLE_BINDING) readonly buffer PrismSamples {
  vec2 prismSamples[];
};
layout(std430, set = 4, binding = PRISM_PATCH_BINDING) readonly buffer PrismPatches {
  PrismPatch prismPatches[];
};
#endif
#endif
