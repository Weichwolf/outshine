#version 450
#extension GL_GOOGLE_include_directive : require
#include "fullscreen.glsl"

void main() {
  gl_Position = fullscreenPosition();
}
