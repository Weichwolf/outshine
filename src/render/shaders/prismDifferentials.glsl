#include "prismPlane.glsl"
vec4 prismDifferenceOfProducts(vec4 a, vec4 b, vec4 c, vec4 d) {
  precise vec4 cd = c * d;
  precise vec4 error = fma(-c, d, cd);
  precise vec4 value = fma(a, b, -cd);
  precise vec4 result = value + error;
  return result;
}

void prismPositionDifferentials(mat4 b, vec3 normal, float height, vec3 u, vec3 v,
                                vec2 ndc, vec2 viewportSize, out vec3 dx, out vec3 dy) {
  // Power-of-two scaling preserves the projective query while bounding intermediate products.
  vec4 maximum = max(max(abs(b[0]), abs(b[1])), max(abs(b[2]), abs(b[3])));
  float largest = max(max(maximum.x, maximum.y), max(maximum.z, maximum.w));
  int exponent;
  frexp(largest, exponent);
  for (int i = 0; i < 4; ++i) { b[i] = ldexp(b[i], ivec4(-exponent)); }
  uint axis = prismPlaneAxis(normal);
  vec4 n = vec4(b[0][axis], b[1][axis], b[2][axis], b[3][axis]);
  vec4 w = vec4(b[0].w, b[1].w, b[2].w, b[3].w);
  vec2 t0 = vec2(dot(u, b[0].xyz), dot(v, b[0].xyz));
  vec2 t1 = vec2(dot(u, b[1].xyz), dot(v, b[1].xyz));
  vec2 t2 = vec2(dot(u, b[2].xyz), dot(v, b[2].xyz));
  vec2 t3 = vec2(dot(u, b[3].xyz), dot(v, b[3].xyz));
  // Factor the plane homography minors before rounding translated coordinate coefficients.
  vec4 m0 = prismDifferenceOfProducts(vec4(t0, t0), vec4(w.yy, w.zz),
                                      vec4(t1, t2), vec4(w.x));
  vec4 m1 = prismDifferenceOfProducts(vec4(t1, t0), vec4(w.zz, w.ww),
                                      vec4(t2, t3), vec4(w.yy, w.xx));
  vec4 m2 = prismDifferenceOfProducts(vec4(t1, t3), vec4(w.ww, w.zz),
                                      vec4(t3, t2), vec4(w.yy, w.ww));
  vec3 denominator = prismDifferenceOfProducts(vec4(n.z), vec4(w.xyw, 0.0),
                                               vec4(n.xyw, 0.0), vec4(w.z)).xyz;
  precise vec2 k01 = fma(vec2(n.z), m0.xy, fma(vec2(-n.y), m0.zw, n.x * m1.xy));
  precise vec2 k03 = fma(vec2(n.z), m1.zw, fma(vec2(-n.w), m0.zw, n.x * m2.zw));
  precise vec2 k13 = fma(vec2(n.z), m2.xy, fma(vec2(-n.w), m1.xy, n.y * m2.zw));
  precise float weight = fma(denominator.x, ndc.x, fma(denominator.y, ndc.y, denominator.z));
  precise float incidence = fma(-height, w.z, n.z);
  precise vec2 localDx = (fma(k01, vec2(ndc.y), k03) / weight) * (incidence / weight) * (2.0 / viewportSize.x);
  precise vec2 localDy = (fma(-k01, vec2(ndc.x), k13) / weight) * (incidence / weight) * (-2.0 / viewportSize.y);
  dx = u * localDx.x + v * localDx.y;
  dy = u * localDy.x + v * localDy.y;
}
