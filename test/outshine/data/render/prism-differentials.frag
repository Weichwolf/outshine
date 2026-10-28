#version 450
#extension GL_GOOGLE_include_directive : require
#include "render/shaders/prismDifferentialVaryings.glsl"
layout(location = 0) flat in vec4 testPlane;
layout(location = 1) flat in vec4 testPixel;
layout(location = 2) flat in uint testIndex;
layout(location = 3) flat in uint testProjected;
layout(location = 0) out vec4 resultDx;
layout(location = 1) out vec4 resultDy;
void main() {
  vec3 dx = vec3(uintBitsToFloat(0x7fc00001u));
  vec3 dy = dx;
  if (testProjected != 0u) {
    prismPositionDifferentials(readPrismDifferentials(), testPlane.xyz, testPlane.w,
                                testPixel.xy, testPixel.zw, dx, dy);
  }
  resultDx = vec4(dx, float(testProjected));
  resultDy = vec4(dy, float(testIndex));
}
