#version 450
layout(std140, set = 0, binding = 0) uniform Value { vec4 value; };
layout(location = 0) out vec4 colour;
void main() {
  colour = value;
}
