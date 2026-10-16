#ifndef OUTSHINE_CLOUD_VIEW_BOUNDARY_GLSL
#define OUTSHINE_CLOUD_VIEW_BOUNDARY_GLSL

AirPathSpan cloudViewBoundarySpan(Medium medium, vec3 originM, vec3 direction) {
  vec3 originKm = originM * 0.001;
  float radiusKm = medium.BottomRadiusKm;
  float topHeightKm = medium.TopRadiusKm - radiusKm;
  float halfLinear = fma(radiusKm, direction.y, dot(originKm, direction));
  float groundConstant = fma(2.0 * radiusKm, originKm.y, dot(originKm, originKm));
  if (groundConstant < 0.0) { return AirPathSpan(0.0, 0.0); }
  float topConstant = fma(-topHeightKm, 2.0 * radiusKm + topHeightKm, groundConstant);
  AirRoots top = airSolveQuadratic(AirQuadratic(1.0, halfLinear, topConstant));
  float beginKm = max(0.0, min(top.First, top.Second));
  float endKm = max(top.First, top.Second);
  if (halfLinear < 0.0) {
    AirRoots ground = airSolveQuadratic(AirQuadratic(1.0, halfLinear, groundConstant));
    float hitKm = min(ground.First, ground.Second);
    if (hitKm >= 0.0) { endKm = min(endKm, hitKm); }
  }
  return endKm > beginKm ? AirPathSpan(beginKm, endKm) : AirPathSpan(0.0, 0.0);
}

#endif
