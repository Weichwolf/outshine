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

void main() {
  const uint corners[36] = uint[36](0,4,6,0,6,2,1,3,7,1,7,5,
                                    0,1,5,0,5,4,2,6,7,2,7,3,
                                    0,2,3,0,3,1,4,5,7,4,7,6);
  PrismPatch gridPatch = prismPatches[gl_InstanceIndex];
  uint vertex = uint(gl_VertexIndex);
  if (determinant(mat3(gridPatch.current)) < 0.0) {
    uint within = vertex % 3u;
    vertex = vertex - within + (within == 0u ? 0u : 3u - within);
  }
  uint corner = corners[vertex];
  vec3 fraction = vec3(float(corner & 1u), float((corner >> 1u) & 1u), float(corner >> 2u));
  vec3 point = fraction * vec3(gridPatch.bounds.y, gridPatch.bounds.x - gridPatch.shape.w, gridPatch.bounds.z);
  point.y += gridPatch.shape.w;
  mat4 model = gridPatch.current;
  model[3] += vec4(s.shift.xyz, 0.0);
#if PRISM_DEPTH
  mat4 clipFromLocal = s.lightFromWorld * model;
#else
  mat4 clipFromLocal = s.viewProj * model;
#endif
  gl_Position = clipFromLocal * vec4(point, 1.0);
  prismLocalFromClip = inverse(clipFromLocal);
  prismPatchIndex = uint(gl_InstanceIndex);
#if !PRISM_DEPTH
  writePrismDifferentials(prismLocalFromClip);
#endif
}
