#version 450

layout(set = 1, binding = 0) uniform Object {
  uvec4 colour;
} object;
layout(set = 2, binding = 0) uniform sampler2D imageMap;
layout(location = 0) out vec4 pixel;

void main() {
  pixel = vec4(vec3(object.colour.xyz), 1.0) * texture(imageMap, vec2(0.5));
}
