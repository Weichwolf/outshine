#version 450
#extension GL_GOOGLE_include_directive : require
#include "fullscreen.glsl"

layout(std140, set = 1, binding = 0) uniform GeometryDepth {
  vec4 Clip;
}

pushed;

void main() {
  gl_Position = fullscreenPosition();
  gl_Position.z = pushed.Clip.x;
}
