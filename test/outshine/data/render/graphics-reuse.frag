#version 450
layout(location = 0) in vec2 uv;
layout(set = 0, binding = 0, std140) uniform Settings { vec4 Tint; } settings;
layout(location = 0) out vec4 colour;
void main() { colour = vec4(uv, 0.25, 1.0) * settings.Tint; }
