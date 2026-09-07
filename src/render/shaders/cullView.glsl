layout(std140, set = 2, binding = 0) uniform CullView {
vec4 planes[6];
  vec4 shift;
  uint jobs;

  float errorPerMetre;
  uint pad0, pad1;

  mat4 clip;
  uvec4 pyramidWide;
  uvec4 pyramidHigh;
  uvec4 pyramidAt;
  uint occludes;
  uint pad2, pad3, pad4;
} view;
