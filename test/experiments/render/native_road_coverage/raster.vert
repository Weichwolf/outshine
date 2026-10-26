#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in uint primitiveIdentity;
layout(location = 0) flat out uint identity;

layout(std430, set = 0, binding = 0) readonly buffer Models {
  mat4 models[];
};

layout(std140, set = 1, binding = 0) uniform Frame {
  mat4 clip;
  vec4 shift;
}

frame;

void main() {
  identity = primitiveIdentity;
  gl_Position =
      frame.clip * vec4((models[gl_InstanceIndex] * vec4(position, 1)).xyz + frame.shift.xyz, 1);
}
