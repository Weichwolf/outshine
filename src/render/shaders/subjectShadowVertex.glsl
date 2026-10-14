#version 450
#extension GL_GOOGLE_include_directive : require
#include "depthView.glsl"
#include "subjectPlacement.glsl"
layout(location = 0) in vec3 position;
#if SHADOW_UVS > 0
layout(location = 1) in vec2 vertexUv;
#endif
#if SHADOW_UVS > 1
layout(location = 2) in vec2 vertexUv1;
#endif
#if SHADOW_TINTED
layout(std430, set = 0, binding = 1) readonly buffer ColourFactors { vec4 colourFactors[]; };
#endif
layout(location = 0) out vec2 uv;
layout(location = 1) out vec2 uv1;
layout(location = 2) out float coverage;
void main() {
  GpuPlacement placement = placements[uint(gl_InstanceIndex)];
  mat4 model = placement.current;
  model[3] += vec4(s.shift.xyz, 0.0);
  gl_Position = s.lightFromWorld * (model * vec4(position, 1.0));
  uv = vec2(0.0);
  uv1 = vec2(0.0);
  coverage = 1.0;
#if SHADOW_UVS > 0
  uv = vertexUv;
#endif
#if SHADOW_UVS > 1
  uv1 = vertexUv1;
#endif
#if SHADOW_TINTED
  coverage = colourFactors[placement.colourOffset + uint(gl_VertexIndex)].a;
#endif
}
