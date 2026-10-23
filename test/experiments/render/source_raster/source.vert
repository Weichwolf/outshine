#version 450
layout(std430, set = 0, binding = 0) readonly buffer Positions { vec4 points[]; } source;
void main() {
  const uint indices[6] = uint[6](0, 1, 2, 0, 2, 3);
  gl_Position = source.points[uint(gl_InstanceIndex) * 4u + indices[gl_VertexIndex]];
}
