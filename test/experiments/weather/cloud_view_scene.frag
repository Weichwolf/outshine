#version 450
layout(location = 0) out vec4 colour;

layout(std140, set = 3, binding = 0) uniform Surface {
  vec4 Lit;
}

surface;

void main() {
  colour = surface.Lit;
}
