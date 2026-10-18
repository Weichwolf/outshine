#version 450
layout(set = 0, binding = 0, std140) uniform Parameters { vec4 colour; vec4 state; } params;
layout(location = 0) out vec4 first;
layout(location = 1) out vec4 second;
void main() {
  first = params.colour;
  second = vec4(params.colour.brg, params.colour.a);
}
