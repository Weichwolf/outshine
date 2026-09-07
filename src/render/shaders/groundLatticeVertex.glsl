#version 450
#extension GL_GOOGLE_include_directive : require
#define VARYING out
#include "subjectVaryings.glsl"
#include "subjectView.glsl"
#include "groundLatticeCore.glsl"
layout(set = 0, binding = 0) uniform sampler2DArray pages;
void main() {
  GroundPoint g = groundPointAt(pages);
  mat4 stood = mat4(c0, c1, c2, c3);
  mat4 model = mat4(c0, c1, c2, c3 + vec4(s.shift.xyz, 0.0));
  vec4 local = vec4(g.local, 1.0);
  vec4 frame = stood * local;
  vec4 wp = model * local;
  gl_Position = s.viewProj * wp;
  uv = vec2(frame.x, -frame.z);
  uv1 = vec2(0.0);
  colour = vec4(1.0);
  normal = normalize(model[0].xyz * g.normal.x + model[1].xyz * g.normal.y + model[2].xyz * g.normal.z);
  position = wp.xyz;
  localPosition = g.local;
  lightSpace = s.lightFromWorld * (model * vec4(g.rim, 1.0));
#if SUBJECT_WRITES_VELOCITY
  curClip = gl_Position;
  prevClip = s.prevViewProj * (mat4(c0, c1, c2, c3 + vec4(s.prevShift.xyz, 0.0)) * local);
#endif
}
