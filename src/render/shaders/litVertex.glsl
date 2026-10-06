#version 450
#extension GL_GOOGLE_include_directive : require
#include "subjectView.glsl"
#define VARYING out
#define GROUND_WORLD_POSITION
#include "subjectVaryings.glsl"
#if LIT_MAPPED
layout(location = 9) out vec4 tangent;
#endif
#include "subjectPlacement.glsl"
layout(location = 0) in vec3 p;
layout(location = 3) in vec3 vertexNormal;
#if LIT_UVS > 0
layout(location = 1) in vec2 vertexUv;
#endif
#if LIT_UVS > 1
layout(location = 6) in vec2 vertexUv1;
#endif
#if LIT_TINTED
layout(std430, set = 0, binding = 1) readonly buffer ColourFactors { vec4 colourFactors[]; };
#endif
#if LIT_MAPPED
#if LIT_TINTED
layout(std430, set = 0, binding = 2) readonly buffer TangentFactors { vec4 tangentFactors[]; };
#else
layout(std430, set = 0, binding = 1) readonly buffer TangentFactors { vec4 tangentFactors[]; };
#endif
#endif
#if SUBJECT_WRITES_VELOCITY
layout(location = 5) in vec3 previous;
#endif
void main() {
  GpuPlacement placement = placements[gl_InstanceIndex];
  mat4 m = placement.current;
  vec4 stable = m * vec4(p, 1.0);
  vec4 world = stable + vec4(s.shift.xyz, 0.0);
  gl_Position = s.viewProj * world;
  position = world.xyz;
  groundWorldM = vec3(stable.x, stable.y, -stable.z);
  localPosition = p;
  lightSpace = s.lightFromWorld * world;
  normal = normalize(m[0].xyz * vertexNormal.x + m[1].xyz * vertexNormal.y + m[2].xyz * vertexNormal.z);
  uv = vec2(0.0);
  uv1 = vec2(0.0);
  colour = vec4(1.0);
#if LIT_UVS > 0
  uv = vertexUv;
#endif
#if LIT_UVS > 1
  uv1 = vertexUv1;
#endif
#if LIT_TINTED
  colour = colourFactors[placement.colourOffset + uint(gl_VertexIndex)];
#endif
#if LIT_MAPPED
  vec4 vertexTangent = tangentFactors[placement.tangentOffset + uint(gl_VertexIndex)];
  tangent = vec4(normalize(m[0].xyz * vertexTangent.x + m[1].xyz * vertexTangent.y +
                          m[2].xyz * vertexTangent.z), vertexTangent.w);
#endif
#if SUBJECT_WRITES_VELOCITY
  curClip = gl_Position;
  mat4 previousPlacement = placement.previous;
  previousPlacement[3] += vec4(s.prevShift.xyz, 0.0);
  prevClip = s.prevViewProj * (previousPlacement * vec4(previous, 1.0));
#endif
}
