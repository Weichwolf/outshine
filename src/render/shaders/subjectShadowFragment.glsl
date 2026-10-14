#version 450
#extension GL_GOOGLE_include_directive : require
#include "material.glsl"
layout(location = 0) in vec2 uv;
layout(location = 1) in vec2 uv1;
layout(location = 2) in float coverage;
#if SHADOW_UVS > 0
layout(set = 2, binding = 0) uniform sampler2D imageMap;
#endif
void main() {
  M surface = materialAt();
  float alpha = 1.0;
#if SHADOW_UVS > 0
  alpha = texture(imageMap, materialColourUv(surface, uv, uv1)).a;
#endif
  if (materialCoverage(surface, alpha, coverage) < surface.cut) { discard; }
}
