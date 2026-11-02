#version 450
#extension GL_GOOGLE_include_directive : require
layout(std430, set = 0, binding = 0) readonly buffer Requests { ivec4 requests[]; };
#include "fieldPageFeedback.glsl"
layout(location = 0) out vec4 feedback;
void main() {
  ivec4 request = requests[uint(gl_FragCoord.y) * 13u + uint(gl_FragCoord.x)];
  feedback = request.w != 0 ? fieldPageFeedback(request.xyz) : vec4(0.0);
}
