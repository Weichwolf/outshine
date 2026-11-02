#include "lighting.glsl"
#if SURFACE_BASE_LOCATION > 0
#define GROUND_CLASS_BINDING 0
#else
#define GROUND_CLASS_BINDING 2
layout(set = 2, binding = 0) uniform sampler2D shadowMap;
#define ENVIRONMENT_SPECULAR_BINDING 1
#include "environmentSpecular.glsl"
#endif
layout(std430, set = 2, binding = GROUND_CLASS_BINDING) readonly buffer GroundClasses { uint groundClasses[]; };
layout(std430, set = 2, binding = GROUND_CLASS_BINDING + 1) readonly buffer GroundPalette { float groundPalette[]; };
#if GROUND_FIELD_PAGES
layout(std430, set = 2, binding = GROUND_CLASS_BINDING + 2) readonly buffer GroundFieldPages {
  uvec4 groundFieldLayout;
  uvec4 groundFieldEntries[];
};
layout(set = 2, binding = GROUND_CLASS_BINDING + 3) uniform sampler2D groundFieldAtlas;
uvec4 fieldPageEntry(uint index) { return groundFieldEntries[index]; }
#endif
