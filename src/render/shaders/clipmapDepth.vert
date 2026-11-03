#version 450
#extension GL_GOOGLE_include_directive : require
#include "geometryView.glsl"
#include "clipmapVertexCore.glsl"

void main() {
  gl_Position = s.viewProj * vec4(clipmapPoint(uint(gl_InstanceIndex)).Position, 1.0);
}
