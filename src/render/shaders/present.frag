#version 450

layout(set = 2, binding = 0) uniform sampler2D frame;
layout(location = 0) out vec4 colour;

void main() {
  colour = texelFetch(frame, ivec2(gl_FragCoord.xy), 0);
}
