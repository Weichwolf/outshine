#version 450
#extension GL_GOOGLE_include_directive : require
#define VARYING in
#include "subjectVaryings.glsl"
#include "subjectOutput.glsl"
#include "material.glsl"
#include "groundBindings.glsl"
#include "brdfTables.glsl"
const float kPi = acos(-1.0);
#include "metalRoughBrdf.glsl"
#include "sheenLobe.glsl"
#include "iridescenceLobe.glsl"
#include "microfacetEnergy.glsl"
#include "subjectLighting.glsl"
#include "groundClass.glsl"
void main() {
  M surface = materialAt();
  vec3 shadingNormal = facing(normal, gl_FrontFacing);
  float edgeM;
  int runnerUp;
  int which = groundClassAt(uv, edgeM, runnerUp);
  uint rows = floatBitsToUint(groundPalette[0]);
  vec2 stepM = fwidth(uv);
  float acrossM = 0.5 * max(max(stepM.x, stepM.y), 1.0e-4);
  float slopeDeg = degrees(acos(clamp(dot(normalize(normal), normalize(lights.up.xyz)), 0.0, 1.0)));
  vec4 wears = edgeM < kGroundNoEdgeM
      ? mix(groundWearsSlope(runnerUp, rows, slopeDeg), groundWearsSlope(which, rows, slopeDeg), smoothstep(-acrossM, acrossM, edgeM))
      : groundWearsSlope(which, rows, slopeDeg);
  vec3 shaded = shadeRow(surface, localPosition, shadingNormal, position, wears.rgb,
      surface.metalness, surface.roughness, vec3(0.0), 0.0, surface.emissive, vec3(0.0), lightSpace, shadowMap);
  outputSurface(vec4(shaded, 1.0), shadingNormal, surface.identity);
}
