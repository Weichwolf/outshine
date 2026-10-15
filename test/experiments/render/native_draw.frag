#version 450

layout(location = 0) in vec2 surfaceUv;
layout(location = 0) out vec4 colour;
layout(std140, set = 2, binding = 0) uniform FragmentParameters { vec4 tint; };
layout(std430, set = 1, binding = 0) readonly buffer Levels { uint levels[]; };
layout(set = 2, binding = 1) uniform sampler2D image;

void main() {
  colour = texture(image, surfaceUv) * tint;
  colour.b += float(levels.length()) / 16.0;
}
