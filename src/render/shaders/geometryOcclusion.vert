#version 450
#extension GL_GOOGLE_include_directive : require
#include "depthView.glsl"
#include "geometryPlacements.glsl"
layout(location = 0) in vec3 position;
void main() {
  const vec4 stable = placements[uint(gl_InstanceIndex)].current * vec4(position, 1.0);
  gl_Position = s.lightFromWorld * (stable + vec4(s.shift.xyz, 0.0));
}
