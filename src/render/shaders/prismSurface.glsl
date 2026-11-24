#include "prismView.glsl"
#include "prismTrace.glsl"
#include "prismDifferentialVaryings.glsl"
#include "surfaceNormal.glsl"

SurfaceDifferential prismSurfaceDifferential(vec3 localChange, mat3 current, vec3 u, vec3 v) {
  SurfaceDifferential result;
  result.uv = vec2(dot(localChange, u), dot(localChange, v));
  result.uv1 = result.uv;
  result.facade = result.uv;
  result.localPosition = localChange;
  result.position = current * localChange;
  result.metric = vec3(result.position.x, result.position.y, -result.position.z);
  return result;
}

void convexPrismDifferentials(vec3 point, vec4 plane, vec2 ndc,
                              out vec3 localDx, out vec3 localDy) {
  mat4 unproject = prismLocalFromClip;
  vec4 w = unproject[2];
  float incidence = dot(w, plane);
  localDx = localDy = vec3(0.0);
  if (incidence == 0.0) { return; }
  vec4 h = unproject * vec4(ndc, 0.0, 1.0);
  float divisor = (h - w * (dot(h, plane) / incidence)).w;
  if (divisor == 0.0) { return; }
  vec4 dx = unproject[0] - w * (dot(unproject[0], plane) / incidence);
  vec4 dy = unproject[1] - w * (dot(unproject[1], plane) / incidence);
  localDx = (dx.xyz - point * dx.w) * (2.0 / (divisor * prismViewport.area.z));
  localDy = (dy.xyz - point * dy.w) * (-2.0 / (divisor * prismViewport.area.w));
}

float prismRoughnessOverride;

bool readPrismSurface() {
  PrismPatch gridPatch = prismPatches[prismPatchIndex];
  vec2 ndc = prismScreenNdc();
  vec3 point, localNormal;
  float height;
  uint surface;
  if (!tracePrism(gridPatch, ndc, point, localNormal, height, surface)) { return false; }
  vec4 stable = gridPatch.current * vec4(point, 1.0);
  vec4 world = stable + vec4(prismView.shift.xyz, 0.0);
  vec4 clip = prismView.viewProj * world;
  if (!(clip.w > 0.0 && clip.z >= 0.0 && clip.z <= clip.w)) { return false; }
  gl_FragDepth = clip.z / clip.w;
  position = world.xyz;
  localPosition = point;
  groundWorldM = vec3(stable.x, stable.y, -stable.z);
  lightSpace = prismView.lightFromWorld * world;
  colour = vec4(1.0);
  prismRoughnessOverride = -1.0;
  normal = transformedSurfaceNormal(mat3(gridPatch.current), localNormal);
  vec3 u, v, localDx, localDy;
  if (gridPatch.samples.w == 0u) {
    prismSurfaceAxes(localNormal, u, v);
    prismPositionDifferentials(readPrismDifferentials(), localNormal, height, ndc,
                               prismViewport.area.zw, localDx, localDy);
  } else {
    uint coating = gridPatch.samples.x + 14u + (((gridPatch.samples.y >> surface) & 1u) << 1u);
    vec4 tint = vec4(prismSamples[coating], prismSamples[coating + 1u]);
    colour.rgb = tint.rgb;
    prismRoughnessOverride = tint.a;
    vec3 n = normalize(localNormal);
    vec3 vertical = normalize(transpose(mat3(gridPatch.current)) * vec3(0, 1, 0));
    vec3 across = cross(vertical, n);
    if (dot(across, across) < 0.0001) {
      vec3 axis = abs(n.x) < 0.9 ? vec3(1, 0, 0) : vec3(0, 0, 1);
      across = cross(axis, n);
    }
    u = normalize(across);
    v = cross(n, u);
    convexPrismDifferentials(point, vec4(localNormal, -height), ndc, localDx, localDy);
  }
#if SURFACE_BASE_LOCATION == 0
  prismShadowReceiver = prismReceiverPlane(mat3(prismView.lightFromWorld), mat3(gridPatch.current), u, v);
#endif
  surfaceDx = prismSurfaceDifferential(localDx, mat3(gridPatch.current), u, v);
  surfaceDy = prismSurfaceDifferential(localDy, mat3(gridPatch.current), u, v);
  uv = vec2(dot(point, u), dot(point, v));
  uv1 = uv;
  facadeBay = uv.x;
  facadeCode = 0.0;
#if LIT_MAPPED
  vec3 tangentDirection = mat3(gridPatch.current) * u;
  tangentDirection = normalize(tangentDirection - normal * dot(normal, tangentDirection));
  float handedness = sign(dot(cross(normal, tangentDirection), mat3(gridPatch.current) * v));
  tangent = vec4(tangentDirection, handedness);
#endif
#if SURFACE_WRITES_VELOCITY
  curClip = clip;
  vec4 previousWorld = gridPatch.previous * vec4(point, 1.0) + vec4(prismView.prevShift.xyz, 0.0);
  prevClip = prismView.prevViewProj * previousWorld;
#endif
  return true;
}
