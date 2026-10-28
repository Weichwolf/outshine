#ifndef OUTSHINE_PRISM_DIFFERENTIALS
#define OUTSHINE_PRISM_DIFFERENTIALS
#include "prismPlane.glsl"
#include "prismProjectiveMath.glsl"
struct PrismDifferentialFrame {
  vec3 xy;
  vec3 xw;
  vec3 yw;
  mat3 weight;
  vec4 camera;
  ivec3 scale;
};

vec4 prismDifferenceOfProducts(vec4 a, vec4 b, vec4 c, vec4 d) {
  precise vec4 cd = c * d;
  precise vec4 error = fma(-c, d, cd);
  precise vec4 value = fma(a, b, -cd);
  precise vec4 result = value + error;
  return result;
}

PrismDifferentialFrame preparePrismDifferentials(mat4 b) {
  // Balance local chart axes independently so small representable gradients survive the minors.
  ivec4 exponent;
  b = prismNormalizeRows(b, exponent);
  vec3 m01 = prismDifferenceOfProducts(b[0], vec4(b[1].w), b[1], vec4(b[0].w)).xyz;
  vec3 m02 = prismDifferenceOfProducts(b[0], vec4(b[2].w), b[2], vec4(b[0].w)).xyz;
  vec3 m12 = prismDifferenceOfProducts(b[1], vec4(b[2].w), b[2], vec4(b[1].w)).xyz;
  vec3 m03 = prismDifferenceOfProducts(b[0], vec4(b[3].w), b[3], vec4(b[0].w)).xyz;
  vec3 m13 = prismDifferenceOfProducts(b[1], vec4(b[3].w), b[3], vec4(b[1].w)).xyz;
  vec3 m32 = prismDifferenceOfProducts(b[3], vec4(b[2].w), b[2], vec4(b[3].w)).xyz;
  // Store the three independent entries of each antisymmetric plane-homography minor.
  precise vec3 xy = fma(b[2].zxy, m01.yzx, fma(-b[1].zxy, m02.yzx, b[0].zxy * m12.yzx));
  precise vec3 xw = fma(b[2].zxy, m03.yzx, fma(-b[3].zxy, m02.yzx, b[0].zxy * m32.yzx));
  precise vec3 yw = fma(b[2].zxy, m13.yzx, fma(-b[3].zxy, m12.yzx, b[1].zxy * m32.yzx));
  vec3 w0 = prismDifferenceOfProducts(b[2], vec4(b[0].w), b[0], vec4(b[2].w)).xyz;
  vec3 w1 = prismDifferenceOfProducts(b[2], vec4(b[1].w), b[1], vec4(b[2].w)).xyz;
  vec3 w3 = prismDifferenceOfProducts(b[2], vec4(b[3].w), b[3], vec4(b[2].w)).xyz;
  return PrismDifferentialFrame(xy, xw, yw, mat3(w0, w1, w3), b[2], exponent.xyz - ivec3(exponent.w));
}

void prismPositionDifferentials(PrismDifferentialFrame frame, vec3 normal, float height,
                                vec2 ndc, vec2 viewportSize, out vec3 dx, out vec3 dy) {
  uint axis = prismPlaneAxis(normal);
  precise float weight = fma(frame.weight[0][axis], ndc.x,
                              fma(frame.weight[1][axis], ndc.y, frame.weight[2][axis]));
  float localHeight = ldexp(height, -frame.scale[axis]);
  precise float incidence = fma(-localHeight, frame.camera.w, frame.camera[axis]);
  precise vec3 localDx = cross(abs(normal), fma(frame.xy, vec3(ndc.y), frame.xw));
  precise vec3 localDy = cross(abs(normal), fma(-frame.xy, vec3(ndc.x), frame.yw));
  dx = ldexp((localDx / weight) * (incidence / weight) * (2.0 / viewportSize.x), frame.scale);
  dy = ldexp((localDy / weight) * (incidence / weight) * (-2.0 / viewportSize.y), frame.scale);
}
#endif
