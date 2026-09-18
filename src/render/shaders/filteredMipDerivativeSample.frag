#version 450

layout(set = 2, binding = 0) uniform sampler2D source;
layout(location = 0) in vec2 sampledUv;
layout(location = 0) out vec4 colour;

void main() {
  colour = texture(source, sampledUv);
}
