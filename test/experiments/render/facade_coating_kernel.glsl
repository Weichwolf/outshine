#include "surfaceNoise.glsl"

float facadeCoatingVisibility(vec3 dxM, vec3 dyM) {
  float size = max(length(dxM), length(dyM)) / 1.5;
  return 1.0 - smoothstep(1.0, 2.0, size);
}

vec4 facadeCoatedFootprint(vec2 fractions, vec2 at, vec2 dx, vec2 dy, float storeysPerM,
                           vec3 positionM, vec3 dxM, vec3 dyM) {
  float visibility = facadeCoatingVisibility(dxM, dyM);
  if (visibility == 0.0 || (fractions.x == 1.0 && fractions.y == 1.0)) {
    return vec4(facadeFootprintCoverage(fractions, at, dx, dy, storeysPerM), 0.0);
  }
  at = fract(at);
  vec2 width = abs(dx) + abs(dy);
  vec4 result = vec4(0.0);
  for (int y = 0; y < 3; ++y) {
    for (int x = 0; x < 3; ++x) {
      vec2 offset = (vec2(x, y) - 1.0) / 3.0;
      vec3 regions = facadeBoxCoverage(
          fractions, at + offset.x * dx + offset.y * dy, width / 3.0, storeysPerM);
      float wall = max(1.0 - regions.x - 0.35 * regions.z, 0.0);
      vec3 pointM = positionM + offset.x * dxM + offset.y * dyM;
      float variation = surfaceNoise3(pointM / 1.5).w - 0.5;
      result += vec4(regions, wall * variation);
    }
  }
  result /= 9.0;
  if ((dx.y == 0.0 && dy.x == 0.0) || (dx.x == 0.0 && dy.y == 0.0)) {
    result.xyz = facadeBoxCoverage(fractions, at, width, storeysPerM);
  }
  result.xyz = clamp(result.xyz, 0.0, 1.0);
  result.y = min(result.y, result.x);
  result.z = min(result.z, 1.0 - result.x);
  float bound = 0.5 * max(1.0 - result.x - 0.35 * result.z, 0.0);
  result.w = clamp(result.w, -bound, bound) * visibility;
  return result;
}

FacadeGlazing facadeCoatedWindow(vec4 filtered, vec3 albedo, float roughness) {
  FacadeGlazing result = facadeBandedWindow(filtered.x, filtered.y, filtered.z, albedo, roughness);
  vec3 room = 2.0 * max(min(albedo, vec3(1.0) - albedo), vec3(0.0));
  vec3 amplitude = min(0.16 * max(albedo, vec3(0.0)), room);
  result.albedo += amplitude * filtered.w;
  return result;
}
