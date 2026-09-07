#version 450
#extension GL_GOOGLE_include_directive : require
#include "depthView.glsl"
layout(location = 0) in vec3 position;
layout(std430, set = 0, binding = 0) readonly buffer Placements { mat4 rows[]; };
void main() {
  mat4 model = rows[2u * uint(gl_InstanceIndex)];
  model[3] += vec4(s.shift.xyz, 0.0);
  gl_Position = s.lightFromWorld * (model * vec4(position, 1.0));
}
