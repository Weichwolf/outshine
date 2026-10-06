vec3 surfaceGradient(vec3 n, vec3 positionDx, vec3 positionDy, float heightDx, float heightDy) {
  vec3 r0 = cross(positionDy, n);
  vec3 r1 = cross(n, positionDx);
  float det = dot(positionDx, r0);
  if (abs(det) < 1.0e-8) { return n * 0.0f; }
  return (r0 * heightDx + r1 * heightDy) * sign(det) / abs(det);
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
