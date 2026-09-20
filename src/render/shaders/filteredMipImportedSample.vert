#version 450

layout(std140, set = 1, binding = 0) uniform Transform {
  mat4 viewProj;
  vec4 shift;
} transform;
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 0) out vec2 sampledUv;

void main() {
  gl_Position = transform.viewProj * vec4(position + transform.shift.xyz, 1.0);
  sampledUv = uv;
}
