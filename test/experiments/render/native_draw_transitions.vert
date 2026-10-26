#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 offset;
layout(set = 0, binding = 0, std140) uniform Params { vec4 tint; vec4 control; } params;
layout(set = 3, binding = 1, std430) readonly buffer Shifts { vec2 values[]; } shifts;
layout(location = 0) out vec2 uv;
layout(location = 1) flat out uint instance;
void main() {
  gl_Position = vec4(position + offset + shifts.values[gl_InstanceIndex] + params.control.xy,
                     params.control.z, 1.0);
  uv = vec2(position.x + 1.0, 1.0 - position.y) * 0.5;
  instance = uint(gl_InstanceIndex);
}
