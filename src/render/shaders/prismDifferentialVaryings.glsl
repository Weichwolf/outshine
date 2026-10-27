#include "prismDifferentials.glsl"
#if PRISM_VERTEX
#define PRISM_DIFFERENTIAL_VARYING out
#else
#define PRISM_DIFFERENTIAL_VARYING in
#endif
layout(location = 5) flat PRISM_DIFFERENTIAL_VARYING vec3 prismDifferentialXY;
layout(location = 6) flat PRISM_DIFFERENTIAL_VARYING vec3 prismDifferentialXW;
layout(location = 7) flat PRISM_DIFFERENTIAL_VARYING vec3 prismDifferentialYW;
layout(location = 8) flat PRISM_DIFFERENTIAL_VARYING mat3 prismDifferentialWeight;
layout(location = 11) flat PRISM_DIFFERENTIAL_VARYING vec4 prismDifferentialCamera;
#undef PRISM_DIFFERENTIAL_VARYING
#if PRISM_VERTEX
void writePrismDifferentials(mat4 localFromClip) {
  PrismDifferentialFrame frame = preparePrismDifferentials(localFromClip);
  prismDifferentialXY = frame.xy;
  prismDifferentialXW = frame.xw;
  prismDifferentialYW = frame.yw;
  prismDifferentialWeight = frame.weight;
  prismDifferentialCamera = frame.camera;
}
#else
PrismDifferentialFrame readPrismDifferentials() {
  return PrismDifferentialFrame(prismDifferentialXY, prismDifferentialXW,
                                  prismDifferentialYW, prismDifferentialWeight,
                                  prismDifferentialCamera);
}
#endif
