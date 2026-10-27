#ifndef OUTSHINE_PRISM_PLANE
#define OUTSHINE_PRISM_PLANE
uint prismPlaneAxis(vec3 normal) {
  return normal.x != 0.0 ? 0u : (normal.y != 0.0 ? 1u : 2u);
}
float prismPlaneIncidence(mat4 localFromClip, vec3 normal, float height) {
  return fma(-height, localFromClip[2].w, localFromClip[2][prismPlaneAxis(normal)]);
}
void prismSurfaceAxes(vec3 normal, out vec3 u, out vec3 v) {
  if (normal.x != 0.0) {
    u = vec3(0.0, 0.0, -normal.x);
    v = vec3(0.0, 1.0, 0.0);
  } else if (normal.y != 0.0) {
    u = vec3(1.0, 0.0, 0.0);
    v = vec3(0.0, 0.0, -normal.y);
  } else {
    u = vec3(normal.z, 0.0, 0.0);
    v = vec3(0.0, 1.0, 0.0);
  }
}
#endif
