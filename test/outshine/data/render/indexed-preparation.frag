#version 450
layout(location = 0) flat in uint identity;
layout(location = 0) out vec4 colour;
void main() {
  colour = vec4(float((identity % 7u) + 1u) / 8.0, 0.5, 0.25, 1.0);
}
