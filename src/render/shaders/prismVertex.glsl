#version 450
#extension GL_GOOGLE_include_directive : require
#define PRISM_VERTEX 1
#include "prismData.glsl"
#if PRISM_DEPTH
#include "depthView.glsl"
#else
#include "geometryView.glsl"
#endif
layout(location = 0) flat out mat4 prismLocalFromClip;
layout(location = 4) flat out uint prismPatchIndex;
#if !PRISM_DEPTH
#include "prismDifferentialVaryings.glsl"
#endif
#include "prismProxy.glsl"
#include "prismProjectiveMath.glsl"

void main() {
  PrismPatch gridPatch = prismPatches[gl_InstanceIndex];
  mat4 model = gridPatch.current;
  model[3] += vec4(s.shift.xyz, 0.0);
#if PRISM_DEPTH
  mat4 clipFromLocal = s.lightFromWorld * model;
#else
  mat4 clipFromLocal = s.viewProj * model;
#endif
  gl_Position = prismProxyPosition(gridPatch, clipFromLocal, uint(gl_VertexIndex));
  prismLocalFromClip = prismInverseProjection(clipFromLocal);
  prismPatchIndex = uint(gl_InstanceIndex);
#if !PRISM_DEPTH
  writePrismDifferentials(prismLocalFromClip);
#endif
}
