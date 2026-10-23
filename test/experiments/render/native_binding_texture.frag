#version 450
layout(set = 0, binding = 0) uniform sampler2D image;
layout(location = 0) out vec4 colour;
void main() {
  colour = texture(image, vec2(0.5));
}
