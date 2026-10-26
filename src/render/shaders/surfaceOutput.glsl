#ifndef SURFACE_FRONT_FACING
#define SURFACE_FRONT_FACING gl_FrontFacing
#endif
#ifndef SURFACE_BASE_LOCATION
#define SURFACE_BASE_LOCATION 0
#endif
#ifndef SURFACE_METALROUGH_LOCATION
#define SURFACE_METALROUGH_LOCATION 0
#endif
layout(location = 0) out vec4 outColour;
#if SURFACE_BASE_LOCATION > 0
layout(location = SURFACE_BASE_LOCATION) out vec4 outBase;
layout(location = SURFACE_METALROUGH_LOCATION) out vec2 outMetalRough;
#endif
void outputMaterial(vec3 albedo, float roughness, float metalness) {
#if SURFACE_BASE_LOCATION > 0
  outBase = vec4(albedo, 1.0);
  outMetalRough = vec2(metalness, roughness);
#endif
}
#if SURFACE_WRITES_VELOCITY
layout(location = 1) out vec2 outVelocity;
#endif
#if SURFACE_NORMAL_LOCATION > 0
layout(location = SURFACE_NORMAL_LOCATION) out vec4 outNormal;
#endif
#if SURFACE_IDENTITY_LOCATION > 0
layout(location = SURFACE_IDENTITY_LOCATION) out vec4 outIdentity;
#endif
void outputSurface(vec4 shaded, vec3 n, float identity) {
  outColour = shaded;
#if SURFACE_WRITES_VELOCITY
  outVelocity = curClip.xy / curClip.w - prevClip.xy / prevClip.w;
#endif
#if SURFACE_NORMAL_LOCATION > 0
  outNormal = vec4(n, SURFACE_FRONT_FACING ? 1.0 : -1.0);
#endif
#if SURFACE_IDENTITY_LOCATION > 0
  outIdentity = vec4(identity, 0.0, 0.0, 1.0);
#endif
}
