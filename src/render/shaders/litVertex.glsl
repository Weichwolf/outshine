#version 450
#extension GL_GOOGLE_include_directive : require
#include "subjectView.glsl"
#define VARYING out
#include "subjectVaryings.glsl"
#if LIT_MAPPED
layout(location = 9) out vec4 tangent;
layout(location = 4) in vec4 vertexTangent;
#endif
layout(std430, set = 0, binding = 0) readonly buffer Placements { mat4 rows[]; };
layout(location = 0) in vec3 p;
layout(location = 3) in vec3 vertexNormal;
#if LIT_UVS > 0
layout(location = 1) in vec2 vertexUv;
#endif
#if LIT_UVS > 1
layout(location = 6) in vec2 vertexUv1;
#endif
#if LIT_TINTED
layout(location = 7) in vec4 vertexColour;
#endif
#if SUBJECT_WRITES_VELOCITY
layout(location = 5) in vec3 previous;
#endif
void main() {
  mat4 m = rows[2u * gl_InstanceIndex];
  m[3] += vec4(s.shift.xyz, 0.0);
  vec4 world = m * vec4(p, 1.0);
  gl_Position = s.viewProj * world;
  position = world.xyz;
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
  colour = vertexColour;
#endif
#if LIT_MAPPED
  tangent = vec4(normalize(m[0].xyz * vertexTangent.x + m[1].xyz * vertexTangent.y +
                          m[2].xyz * vertexTangent.z), vertexTangent.w);
#endif
#if SUBJECT_WRITES_VELOCITY
  curClip = gl_Position;
  mat4 previousPlacement = rows[2u * gl_InstanceIndex + 1u];
  previousPlacement[3] += vec4(s.prevShift.xyz, 0.0);
  prevClip = s.prevViewProj * (previousPlacement * vec4(previous, 1.0));
#endif
}
