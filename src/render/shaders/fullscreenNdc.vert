#version 450
#extension GL_GOOGLE_include_directive : require
#include "fullscreen.glsl"
layout(location = 0) out vec2 ndc;

void main() {
  gl_Position = fullscreenPosition();
  ndc = gl_Position.xy;
}
