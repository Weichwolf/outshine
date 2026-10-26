#version 450
#extension GL_GOOGLE_include_directive : require
layout(location = 0) in vec2 uv;
layout(location = 1) in vec2 uv1;
#if PHASE_SEPARATED
layout(location = 11) in float facadeBay;
layout(location = 12) flat in float facadeCode;
#endif
#include "material.glsl"
layout(location = 0) out vec4 outColour;
layout(location = 1) out vec4 outUv;
layout(location = 2) out vec4 outPhase;
void main() {
  outColour = materialAt().base;
#if PHASE_SEPARATED
  outColour.a = facadeCode;
#else
  outColour.a = floor(uv.x / 256.0);
#endif
  outUv = vec4(uv, 0.0, 1.0);
#if PHASE_SEPARATED
  outPhase = vec4(facadeBay, uv1, 1.0);
#else
  outPhase = vec4(mod(uv.x, 256.0), uv1, 1.0);
#endif
}
