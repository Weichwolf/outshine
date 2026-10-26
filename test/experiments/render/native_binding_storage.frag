#version 450
layout(std430, set = 0, binding = 0) readonly buffer Values { vec4 values[]; };
layout(location = 0) out vec4 colour;
void main() {
  colour = values[0] + vec4(0.0, 0.0, 0.0, float(values.length()));
}
