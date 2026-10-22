#version 450
layout(set = 0, binding = 0, std140) uniform Parameters { vec4 colour; vec4 state; } params;
void main() {
  const vec2 vertices[3] = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1,3));
  gl_Position = vec4(vertices[gl_VertexIndex], params.state.x, 1);
}
