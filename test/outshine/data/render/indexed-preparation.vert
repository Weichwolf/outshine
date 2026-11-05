#version 450
layout(location = 0) flat out uint identity;
void main() {
  const vec2 points[3] = vec2[3](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
  gl_Position = vec4(points[gl_VertexIndex], 0.5, 1);
  identity = uint(gl_InstanceIndex);
}
