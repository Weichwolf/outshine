#version 450
#extension GL_GOOGLE_include_directive : require
#include "depthView.glsl"
#include "subjectPlacement.glsl"
#define FIELD_DEPTH 1
#include "fieldInput.glsl"
layout(location = 0) out vec3 fieldLocalPosition;
layout(location = 1) flat out uvec4 fieldFace;
layout(location = 2) flat out float fieldRoofLevel;

void main() {
  mat4 model = placements[gl_InstanceIndex].current;
  model[3] += vec4(s.shift.xyz, 0.0);
  gl_Position = s.lightFromWorld * (model * vec4(fieldPosition, 1.0));
  fieldLocalPosition = fieldPosition;
  fieldFace = fieldDescriptor();
  fieldRoofLevel = fieldLevelReserved.x;
}
