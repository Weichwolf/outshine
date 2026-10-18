#version 450
layout(set = 1, binding = 0) uniform Depths { vec4 values; } inputs;
void main() {
  vec2 positions[3] = vec2[](vec2(-0.75, -0.75), vec2(0.75, -0.75), vec2(-0.75, 0.75));
  float w = inputs.values.w;
  gl_Position = vec4(positions[gl_VertexIndex] * w, inputs.values[gl_VertexIndex] * w, w);
}
