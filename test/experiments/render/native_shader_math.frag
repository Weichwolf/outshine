#version 450
#extension GL_GOOGLE_include_directive : require
#include "native_shader_math.glsl"
layout(location = 0) flat in vec4 vertexResult;
layout(location = 0) out vec4 vertexColour;
layout(location = 1) out vec4 fragmentColour;
void main() {
  vertexColour = vertexResult;
  fragmentColour = mathChecks();
  fragmentColour.y = inputs.arithmetic.w / inputs.arithmetic.w;
}
