#version 450

layout(location = 0) flat in uint instanceIndex;
layout(location = 0) out vec4 colour;

void main() {
  colour = vec4(gl_FragCoord.x / 8.0, gl_FragCoord.y / 4.0, float(instanceIndex) / 4.0, 1.0);
}
