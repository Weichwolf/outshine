#version 450
#extension GL_GOOGLE_include_directive : require
#define VARYING in
#include "subjectVaryings.glsl"
layout(location = 11) in float facadeBay;
layout(location = 12) flat in float facadeCode;
#include "material.glsl"
#include "surfaceGradient.glsl"
#include "periodicBand.glsl"
#include "facadeOpenings.glsl"
#include "facadeBands.glsl"
#include "facadeFootprint.glsl"
#include "facadeProfiles.glsl"
#include "facadeMasonry.glsl"
layout(location = 0) out vec4 outAt;
layout(location = 1) out vec4 outDx;
layout(location = 2) out vec4 outDy;
void main() {
  vec2 at = vec2(facadeBay, uv.y - 1.0);
  vec2 dx = dFdx(at);
  vec2 dy = dFdy(at);
  vec2 dxM = dFdx(uv1);
  vec2 dyM = dFdy(uv1);
#if MASONRY_DIAGNOSTIC == 0
  outAt = vec4(at, uv1) * materialAt().base.a;
  outDx = vec4(dx, dxM);
  outDy = vec4(dy, dyM);
#else
  vec2 fractions = facadeFractions(int(facadeCode) % kFacadeStyleCount);
  float perM = length(surfaceGradient(normalize(normal), dFdx(position), dFdy(position), dx.y, dy.y));
  if (uv.y < 1.0 || fractions.x <= 0.0 || fractions.y <= 0.0) {
    fractions = vec2(0.0);
    perM = 0.0;
  }
  vec3 regions = facadeFootprintCoverage(fractions, at, dx, dy, perM);
  float field = facadeMasonryField(fractions, at, dx, dy, perM, uv1, dxM, dyM, regions);
  outAt = vec4(regions, field) * materialAt().base.a;
  outDx = vec4(perM, fractions, 1.0);
  outDy = vec4(0.0);
#endif
}
