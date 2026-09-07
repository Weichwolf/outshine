#include "lighting.glsl"
layout(set = 2, binding = 0) uniform sampler2D shadowMap;
layout(std430, set = 2, binding = 1) readonly buffer GroundClasses { uint groundClasses[]; };
layout(std430, set = 2, binding = 2) readonly buffer GroundPalette { float groundPalette[]; };

#define SKY_IRRADIANCE_BINDING 3
#include "skyIrradiance.glsl"
