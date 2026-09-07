#include "lighting.glsl"
#if LIT_TEXTURED
layout(set = 2, binding = 0) uniform sampler2D colourMap;
#if LIT_MAPPED
layout(set = 2, binding = 1) uniform sampler2D normalMap;
#endif
layout(set = 2, binding = 1 + LIT_MAPPED) uniform sampler2D metalRoughMap;
layout(set = 2, binding = 2 + LIT_MAPPED) uniform sampler2D emissiveMap;
layout(set = 2, binding = 3 + LIT_MAPPED) uniform sampler2D specularStrengthMap;
layout(set = 2, binding = 4 + LIT_MAPPED) uniform sampler2D specularTintMap;
#define LIT_NEXT_BINDING (5 + LIT_MAPPED)
#else
#define LIT_NEXT_BINDING 0
#endif
#if LIT_KIND == 3
layout(set = 2, binding = LIT_NEXT_BINDING) uniform sampler2D behindMap;
layout(set = 2, binding = LIT_NEXT_BINDING + 1) uniform sampler2D shadowMap;
#else
layout(set = 2, binding = LIT_NEXT_BINDING) uniform sampler2D shadowMap;
#endif

#if LIT_KIND == 3
#define SKY_IRRADIANCE_BINDING (LIT_NEXT_BINDING + 2)
#else
#define SKY_IRRADIANCE_BINDING (LIT_NEXT_BINDING + 1)
#endif
#include "skyIrradiance.glsl"
