#version 450
layout(location = 0) flat in vec4 measured;
layout(location = 0) out vec4 result;
void main() { result = measured; }
