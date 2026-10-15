#ifndef OUTSHINE_AIR_RAY_GLSL
#define OUTSHINE_AIR_RAY_GLSL
#include "medium.glsl"
#include "world/sky/AirRayLayout.h"

#include "world/sky/AirPathLayout.h"
#define AIR_INLINE
#define AIR_NOEXCEPT
#define MEDIUM_ARG Medium
#include "world/sky/AirPathCore.h"
#undef MEDIUM_ARG
#undef AIR_NOEXCEPT
#undef AIR_INLINE

vec2 airQuadraticRoots(float quadratic, float halfLinear, float constant) {
  AirRoots roots = airSolveQuadratic(AirQuadratic(quadratic, halfLinear, constant));
  return vec2(roots.First, roots.Second);
}

vec2 airShellRoots(AirRay ray, float shellRadiusKm) {
  AirRoots roots = airSphereRoots(ray, shellRadiusKm);
  return vec2(roots.First, roots.Second);
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
