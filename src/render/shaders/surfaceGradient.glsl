float surfaceDerivativeScale(vec3 positionDx, vec3 positionDy) {
  vec3 extent = max(abs(positionDx), abs(positionDy));
  return max(max(extent[0], extent[1]), extent[2]);
}

vec3 surfaceGradient(vec3 n, vec3 positionDx, vec3 positionDy, float heightDx, float heightDy) {
  float scale = surfaceDerivativeScale(positionDx, positionDy);
  if (scale == 0.0) { return n * 0.0f; }
  vec3 dx = positionDx / scale;
  vec3 dy = positionDy / scale;
  vec3 r0 = cross(dy, n);
  vec3 r1 = cross(n, dx);
  float det = dot(dx, r0);
  if (abs(det) < 1.0e-8) { return n * 0.0f; }
  return (r0 * (heightDx / scale) + r1 * (heightDy / scale)) * sign(det) / abs(det);
}

vec3 bumpNormal(vec3 n, vec3 shadingNormal, vec3 positionDx, vec3 positionDy,
                float heightDx, float heightDy) {
  if (heightDx == 0.0 && heightDy == 0.0) { return shadingNormal; }
  vec3 gradient = surfaceGradient(n, positionDx, positionDy, heightDx, heightDy);
  return normalize(shadingNormal - gradient * dot(n, shadingNormal));
}

vec3 facing(vec3 n, bool front) {
  return (front ? normalize(n) : -normalize(n));
}
