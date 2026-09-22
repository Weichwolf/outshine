#version 450
#extension GL_GOOGLE_include_directive : require
#define VARYING in
#define GROUND_WORLD_POSITION
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
#include "groundRock.glsl"
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
  float rockWeight = edgeM < kGroundNoEdgeM
      ? mix(groundRockWeight(runnerUp, rows, slopeDeg), groundRockWeight(which, rows, slopeDeg), smoothstep(-acrossM, acrossM, edgeM))
      : groundRockWeight(which, rows, slopeDeg);
  RockDetail detail = RockDetail(1.0, 0.0, 0.0);
  vec3 worldM = groundWorldM;
  float footprintM = max(length(dFdx(worldM)), length(dFdy(worldM)));
  if (rockWeight > 0.0) {
    detail = rockDetail(worldM, footprintM, rows);
  }
  shadingNormal = rockBumpNormal(shadingNormal, position, detail.BumpM * rockWeight);
  vec3 albedo = min(wears.rgb * mix(1.0, detail.AlbedoScale, rockWeight), vec3(1.0));
  float roughness = clamp(wears.a + detail.RoughnessOffset * rockWeight, 0.04, 1.0);
  vec3 shaded = shadeRow(surface, localPosition, shadingNormal, position, albedo,
      surface.metalness, roughness, vec3(surface.f0), surface.specularWeight, surface.emissive, vec3(0.0), lightSpace, shadowMap);
  outputSurface(vec4(shaded, 1.0), shadingNormal, surface.identity);
}
