#version 450
#extension GL_GOOGLE_include_directive : require
#include "depthView.glsl"
layout(location = 0) in vec3 position;
#include "subjectPlacement.glsl"
void main() {
  mat4 model = placements[uint(gl_InstanceIndex)].current;
  model[3] += vec4(s.shift.xyz, 0.0);
  gl_Position = s.lightFromWorld * (model * vec4(position, 1.0));
}
