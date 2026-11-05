#version 450
layout(set = 2, binding = 0) uniform sampler2D sourceImage;
layout(std140, set = 2, binding = 1) uniform Tint { vec4 tint; };
layout(location = 0) out vec4 colour;
void main() {
  colour = texture(sourceImage, vec2(0.5)) * tint;
}
