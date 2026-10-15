#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec2 instanceOffset;
layout(location = 0) out vec2 surfaceUv;
layout(std140, set = 0, binding = 0) uniform VertexParameters { vec4 transform; vec4 depth; };
layout(std430, set = 1, binding = 0) readonly buffer Levels { uint levels[]; };

void main() {
  gl_Position = vec4(position * transform.xy + transform.zw + instanceOffset,
                     depth.x + float(levels[0]) / 256.0, 1.0);
  surfaceUv = uv;
}
