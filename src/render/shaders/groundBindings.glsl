#include "lighting.glsl"
layout(set = 2, binding = 0) uniform sampler2D shadowMap;
layout(std430, set = 2, binding = 2) readonly buffer GroundClasses { uint groundClasses[]; };
layout(std430, set = 2, binding = 3) readonly buffer GroundPalette { float groundPalette[]; };

#define ENVIRONMENT_SPECULAR_BINDING 1
#include "environmentSpecular.glsl"
#define SKY_IRRADIANCE_BINDING 4
#include "skyIrradiance.glsl"
