#ifndef OUTSHINE_AIR_RAY_GLSL
#define OUTSHINE_AIR_RAY_GLSL
#include "medium.glsl"
#include "world/sky/AirRayLayout.h"

vec2 airQuadraticRoots(float quadratic, float halfLinear, float constant) {
  if (quadratic == 0.0) {
    return vec2(halfLinear == 0.0 ? -1.0 : -constant / (2.0 * halfLinear));
  }
  float discriminant = halfLinear * halfLinear - quadratic * constant;
  if (discriminant < 0.0) { return vec2(-1.0); }
  float root = sqrt(discriminant);
  float stable = -halfLinear - (halfLinear >= 0.0 ? root : -root);
  return stable == 0.0 ? vec2(-halfLinear / quadratic)
                       : vec2(stable / quadratic, constant / stable);
}

vec2 airShellRoots(AirRay ray, float shellRadiusKm) {
  return airQuadraticRoots(1.0, ray.RadiusKm * ray.CosView,
      (ray.RadiusKm - shellRadiusKm) * (ray.RadiusKm + shellRadiusKm));
}

vec2 airShadowRoots(Medium medium, AirRay ray) {
  float sunProjection = ray.RadiusKm * ray.CosSun;
  vec2 roots = airQuadraticRoots(max(0.0, 1.0 - ray.ViewSun * ray.ViewSun),
      ray.RadiusKm * (ray.CosView - ray.CosSun * ray.ViewSun),
      (ray.RadiusKm - medium.BottomRadiusKm) *
          (ray.RadiusKm + medium.BottomRadiusKm) - sunProjection * sunProjection);
  for (int index = 0; index < 2; ++index) {
    if (sunProjection + roots[index] * ray.ViewSun >= 0.0) { roots[index] = -1.0; }
  }
  return roots;
}

bool airSunVisible(Medium medium, AirRay ray, float distanceKm) {
  float heightKm = mediumHeightAlong(medium, ray.RadiusKm, ray.CosView, distanceKm);
  float projection = ray.RadiusKm * ray.CosSun + distanceKm * ray.ViewSun;
  return heightKm >= 0.0 && (projection > 0.0 ||
      projection * projection < heightKm * (heightKm + 2.0 * medium.BottomRadiusKm));
}
#endif
