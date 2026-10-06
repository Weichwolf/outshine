#include "lighting.glsl"
#if SUBJECT_BASE_LOCATION > 0
#define GROUND_CLASS_BINDING 0
#else
#define GROUND_CLASS_BINDING 2
layout(set = 2, binding = 0) uniform sampler2D shadowMap;
#define ENVIRONMENT_SPECULAR_BINDING 1
#include "environmentSpecular.glsl"
#define SKY_IRRADIANCE_BINDING 4
#include "skyIrradiance.glsl"
#endif
layout(std430, set = 2, binding = GROUND_CLASS_BINDING) readonly buffer GroundClasses { uint groundClasses[]; };
layout(std430, set = 2, binding = GROUND_CLASS_BINDING + 1) readonly buffer GroundPalette { float groundPalette[]; };
