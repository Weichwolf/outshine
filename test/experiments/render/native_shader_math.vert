#version 450
#extension GL_GOOGLE_include_directive : require
#include "native_shader_math.glsl"
layout(location = 0) flat out vec4 vertexResult;
void main() {
  vec2 point = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(point * 2.0 - 1.0, 0.5, 1.0);
  vertexResult = mathChecks();
}
