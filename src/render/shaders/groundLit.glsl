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
#if SUBJECT_BASE_LOCATION == 0
#include "subjectLighting.glsl"
#endif
#include "groundClass.glsl"
#include "groundMaterial.glsl"
#include "groundEdge.glsl"
#include "surfaceNoise.glsl"
#include "groundRock.glsl"
#include "groundSubstrate.glsl"
#include "surfaceBlend.glsl"
#include "groundLayers.glsl"
#include "surfaceGradient.glsl"
#if GROUND_PATCHES
#include "groundFineNormal.glsl"
#endif
void main() {
#if GROUND_PATCHES
  vec3 surfaceNormal = groundFineNormal();
#else
  vec3 surfaceNormal = normal;
#endif
  M surface = materialAt();
  vec3 shadingNormal = facing(surfaceNormal, gl_FrontFacing);
  float edgeM;
  float haloM;
  int runnerUp;
  float acrossM = max(groundHalfFootprint(dFdx(uv), dFdy(uv)), 5.0e-5);
  int which = groundMaterialAt(uv, acrossM, edgeM, runnerUp, haloM);
  uint rows = floatBitsToUint(groundPalette[0]);
  float reachM = groundBoundaryReach(which, runnerUp, rows);
  float halfFootprintM = min(acrossM, max(haloM - reachM, 0.0));
  float classWeight = groundEdgeWeight(edgeM, reachM, halfFootprintM);
  float slopeDeg = degrees(acos(clamp(dot(normalize(surfaceNormal), normalize(lights.up.xyz)), 0.0, 1.0)));
  float rockWeight = groundRockWeight(which, rows, slopeDeg);
  float otherRockWeight = edgeM < kGroundNoEdgeM ? groundRockWeight(runnerUp, rows, slopeDeg) : rockWeight;
  vec3 worldM = groundWorldM;
  vec3 worldDx = dFdx(worldM);
  vec3 worldDy = dFdy(worldM);
  float footprintM = max(length(worldDx), length(worldDy));
  int rockRow = int(floatBitsToUint(groundPalette[1]));
  GroundSurface rock = groundSurface(groundWears(rockRow, rows), GroundDetail(1.0, 0.0, vec2(0.0)));
  if (max(rockWeight, otherRockWeight) > 0.0) {
    rock = groundSurface(groundWears(rockRow, rows), rockDetail(worldM, worldDx, worldDy, footprintM, rows));
  }
  vec2 atM = worldM.xz;
  GroundSurface composed = rock;
  if (rockWeight < 1.0) {
    composed = blendGroundSurfaces(groundSubstrateSurface(which, atM, worldDx.xz, worldDy.xz, rows), rock, rockWeight);
  }
  if (edgeM < kGroundNoEdgeM && classWeight < 1.0) {
    GroundSurface other = rock;
    if (otherRockWeight < 1.0) {
      other = blendGroundSurfaces(groundSubstrateSurface(runnerUp, atM, worldDx.xz, worldDy.xz, rows), rock, otherRockWeight);
    }
    composed = blendGroundSurfaces(other, composed, classWeight);
  }
  vec2 heightDxDy = composed.BumpDxDy;
  shadingNormal = bumpNormal(shadingNormal, shadingNormal, dFdx(position), dFdy(position), heightDxDy.x, heightDxDy.y);
  vec3 albedo = clamp(composed.Albedo, vec3(0.0), vec3(1.0));
  float roughness = clamp(composed.Roughness, 0.04, 1.0);
#if SUBJECT_BASE_LOCATION > 0
  outputMaterial(albedo, roughness, surface.metalness);
  outputSurface(vec4(0.0), shadingNormal, surface.identity);
#else
  vec3 shaded = shadeRow(surface, localPosition, shadingNormal, position, albedo,
      surface.metalness, roughness, vec3(surface.f0), surface.specularWeight, surface.emissive, vec3(0.0), lightSpace, shadowMap);
  outputSurface(vec4(shaded, 1.0), shadingNormal, surface.identity);
#endif
}
