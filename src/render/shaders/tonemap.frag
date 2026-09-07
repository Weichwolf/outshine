#version 450
#extension GL_GOOGLE_include_directive : require
#define DISPLAY_BINDING 0
#include "display.glsl"
layout(set = 2, binding = 0) uniform sampler2D scene;
layout(set = 2, binding = 1) uniform sampler2D sceneDepth;
layout(location = 0) out vec4 colour;

void main() {
  ivec2 px = ivec2(gl_FragCoord.xy);
  colour = vec4(displayed(texelFetch(scene, px, 0), texelFetch(sceneDepth, px, 0).r).rgb, 1.0);
}
