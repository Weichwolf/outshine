#version 450
#extension GL_GOOGLE_include_directive : require
#include "fieldMask.glsl"
layout(location = 0) in vec3 fieldLocalPosition;
layout(location = 1) flat in uvec4 fieldFace;
layout(location = 2) flat in float fieldRoofLevel;

void main() {
  if (!fieldOccupied(fieldLocalPosition, fieldFace, fieldRoofLevel)) { discard; }
}
