#version 450
#extension GL_GOOGLE_include_directive : require
#include "subjectView.glsl"
#define VARYING out
#include "flatVaryings.glsl"
#include "subjectPlacement.glsl"
layout(location = 0) in vec3 p;
layout(location = 2) in vec3 emission;
#if FLAT_UVS > 0
layout(location = 1) in vec2 vertexUv;
#endif
#if FLAT_UVS > 1
layout(location = 6) in vec2 vertexUv1;
#endif
#if FLAT_TINTED
layout(std430, set = 0, binding = 1) readonly buffer ColourFactors { vec4 colourFactors[]; };
#endif
#if SUBJECT_WRITES_VELOCITY
layout(location = 5) in vec3 previous;
#endif
void main() {
  GpuPlacement placed = placements[gl_InstanceIndex];
  mat4 placement = placed.current;
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
  colour = colourFactors[placed.colourOffset + uint(gl_VertexIndex)];
#endif
  emitted = emission;
#if SUBJECT_WRITES_VELOCITY
  curClip = gl_Position;
  mat4 previousPlacement = placed.previous;
  previousPlacement[3] += vec4(s.prevShift.xyz, 0.0);
  prevClip = s.prevViewProj * (previousPlacement * vec4(previous, 1.0));
#endif
}
