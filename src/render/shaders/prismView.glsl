#if PRISM_DEPTH
layout(std140, set = 3, binding = 0) uniform PrismView {
  mat4 lightFromWorld;
  vec4 shift;
} prismView;
#else
layout(std140, set = 3, binding = 2) uniform PrismView {
  mat4 viewProj;
  mat4 prevViewProj;
  mat4 lightFromWorld;
  vec4 shift;
  vec4 prevShift;
} prismView;
#endif
#if PRISM_DEPTH
layout(std140, set = 3, binding = 1) uniform PrismViewport {
#else
layout(std140, set = 3, binding = 3) uniform PrismViewport {
#endif
  vec4 area;
} prismViewport;
