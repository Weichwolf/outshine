#version 450

layout(set = 0, binding = 0) uniform View {
  vec4 shift;
} view;

void main() {
  const vec2 corners[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
  gl_Position = vec4(corners[gl_VertexIndex] + view.shift.xy, 0.5, 1.0);
}
