#version 450
layout(location = 0) in vec3 position;
layout(location = 0) out vec3 point;

layout(std140, set = 1, binding = 0) uniform Frame {
  mat4 clip;
  vec4 eye;
  vec4 palette[3];
  uvec4 control;
}

frame;

void main() {
  point = position;
  gl_Position = frame.clip * vec4(position, 1);
}
