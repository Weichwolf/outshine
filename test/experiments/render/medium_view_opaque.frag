#version 450
#extension GL_GOOGLE_include_directive : require
#include "mediumOpaque.glsl"

layout(location = 0) out vec4 colour;
layout(set = 2, binding = 0) uniform sampler2D scene;
layout(set = 2, binding = 1) uniform sampler2D depth;
layout(set = 2, binding = 2) uniform sampler2D multiScatter;
layout(set = 2, binding = 3) uniform sampler2D veil;

layout(std140, set = 3, binding = 2) uniform MediumFrame {
  MediumView View;
}

frame;

void main() {
  colour = mediumClearOpaque(frame.View, gl_FragCoord.xy, scene, depth, multiScatter, veil);
}
