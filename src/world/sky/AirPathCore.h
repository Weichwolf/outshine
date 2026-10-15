#ifndef OUTSHINE_WORLD_SKY_AIRPATHCORE_H
#define OUTSHINE_WORLD_SKY_AIRPATHCORE_H

AIR_INLINE AirRoots airSolveQuadratic(AirQuadratic equation) AIR_NOEXCEPT {
  const float quadratic = equation.Quadratic;
  const float halfLinear = equation.HalfLinear;
  const float constant = equation.Constant;
  if (quadratic == 0.0f) {
    const float root = halfLinear == 0.0f ? -1.0f : -constant / (2.0f * halfLinear);
    return AirRoots(root, root);
  }
  const float discriminant = halfLinear * halfLinear - quadratic * constant;
  if (discriminant < 0.0f) { return AirRoots(-1.0f, -1.0f); }
  const float root = sqrt(discriminant);
  const float stable = -halfLinear - (halfLinear >= 0.0f ? root : -root);
  if (stable == 0.0f) {
    const float repeated = -halfLinear / quadratic;
    return AirRoots(repeated, repeated);
  }
  return AirRoots(stable / quadratic, constant / stable);
}

AIR_INLINE AirRoots airSphereRoots(AirRay ray, float shellRadiusKm) AIR_NOEXCEPT {
  return airSolveQuadratic(
      AirQuadratic(1.0f,
                   ray.RadiusKm * ray.CosView,
                   (ray.RadiusKm - shellRadiusKm) * (ray.RadiusKm + shellRadiusKm)));
}

AIR_INLINE AirPathSpan airPathSpan(MEDIUM_ARG medium, AirRay ray) AIR_NOEXCEPT {
  if (ray.DistanceKm <= 0.0f || ray.RadiusKm < medium.BottomRadiusKm) {
    return AirPathSpan(0.0f, 0.0f);
  }
  const AirRoots top = airSphereRoots(ray, medium.TopRadiusKm);
  const float begin = max(0.0f, min(top.First, top.Second));
  float end = min(ray.DistanceKm, max(top.First, top.Second));
  if (end <= begin) { return AirPathSpan(0.0f, 0.0f); }
  if (ray.CosView < 0.0f) {
    const AirRoots ground = airSphereRoots(ray, medium.BottomRadiusKm);
    const float hit = min(ground.First, ground.Second);
    if (hit >= 0.0f) { end = min(end, hit); }
  }
  return end > begin ? AirPathSpan(begin, end) : AirPathSpan(0.0f, 0.0f);
}

AIR_INLINE AirRay airPathRay(MEDIUM_ARG medium, AirRay ray, AirPathSpan span) AIR_NOEXCEPT {
  if (span.BeginKm > 0.0f) {
    ray.CosView =
        clamp((ray.RadiusKm * ray.CosView + span.BeginKm) / medium.TopRadiusKm, -1.0f, 1.0f);
    ray.CosSun = clamp(
        (ray.RadiusKm * ray.CosSun + span.BeginKm * ray.ViewSun) / medium.TopRadiusKm, -1.0f, 1.0f);
    ray.RadiusKm = medium.TopRadiusKm;
  }
  ray.DistanceKm = max(0.0f, span.EndKm - span.BeginKm);
  return ray;
}

#endif
