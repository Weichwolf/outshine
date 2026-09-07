#version 450
#extension GL_GOOGLE_include_directive : require
#include "subjectView.glsl"
#define VARYING out
#include "flatVaryings.glsl"
layout(std430, set = 0, binding = 0) readonly buffer Placements { mat4 rows[]; };
layout(location = 0) in vec3 p;
layout(location = 2) in vec3 emission;
#if FLAT_UVS > 0
layout(location = 1) in vec2 vertexUv;
#endif
#if FLAT_UVS > 1
layout(location = 6) in vec2 vertexUv1;
#endif
#if FLAT_TINTED
layout(location = 7) in vec4 vertexColour;
#endif
#if SUBJECT_WRITES_VELOCITY
layout(location = 5) in vec3 previous;
#endif
void main() {
  mat4 placement = rows[2u * gl_InstanceIndex];
  placement[3] += vec4(s.shift.xyz, 0.0);
  gl_Position = s.viewProj * (placement * vec4(p, 1.0));
  uv = vec2(0.0);
  uv1 = vec2(0.0);
  colour = vec4(1.0);
#if FLAT_UVS > 0
  uv = vertexUv;
#endif
#if FLAT_UVS > 1
  uv1 = vertexUv1;
#endif
#if FLAT_TINTED
  colour = vertexColour;
#endif
  emitted = emission;
#if SUBJECT_WRITES_VELOCITY
  curClip = gl_Position;
  mat4 previousPlacement = rows[2u * gl_InstanceIndex + 1u];
  previousPlacement[3] += vec4(s.prevShift.xyz, 0.0);
  prevClip = s.prevViewProj * (previousPlacement * vec4(previous, 1.0));
#endif
}
