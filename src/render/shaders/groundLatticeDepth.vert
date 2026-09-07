#version 450
#extension GL_GOOGLE_include_directive : require
#include "depthView.glsl"
#include "groundLatticeCore.glsl"
layout(set = 0, binding = 0) uniform sampler2DArray pages;
void main() {
  GroundPoint g = groundPointAt(pages);
  mat4 model = mat4(c0, c1, c2, c3 + vec4(s.shift.xyz, 0.0));
  gl_Position = s.lightFromWorld * (model * vec4(g.local, 1.0));
}
