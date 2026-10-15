#version 450
layout(location = 0) in vec2 uv;
layout(location = 1) flat in uint instance;
layout(set = 0, binding = 0, std140) uniform Params { vec4 tint; vec4 control; } params;
layout(set = 2, binding = 0) uniform sampler2D image;
layout(location = 0) out vec4 colour;
void main() { colour = texture(image, uv) * params.tint + vec4(0, 0, 0, float(instance) / 8.0); }
