#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 instanceOffset;
layout(location = 0) flat out uint instanceIndex;

void main() {
  gl_Position = vec4(position + instanceOffset, 0.5, 1.0);
  instanceIndex = gl_InstanceIndex;
}
