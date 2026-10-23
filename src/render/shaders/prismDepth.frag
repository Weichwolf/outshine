#version 450
#extension GL_GOOGLE_include_directive : require
#define PRISM_DEPTH 1
#include "prismView.glsl"
#include "prismTrace.glsl"
void main() {
  vec3 point, normal;
  if (!tracePrism(point, normal)) { discard; }
  vec4 world = prismPatches[prismPatchIndex].current * vec4(point, 1.0);
  world.xyz += prismView.shift.xyz;
  vec4 clip = prismView.lightFromWorld * world;
  if (!(clip.w > 0.0 && clip.z >= 0.0 && clip.z <= clip.w)) { discard; }
  gl_FragDepth = clip.z / clip.w;
}
