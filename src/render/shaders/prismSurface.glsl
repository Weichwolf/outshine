#include "prismView.glsl"
#include "prismTrace.glsl"
#include "prismDifferentials.glsl"

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

bool readPrismSurface() {
  vec3 point, localNormal;
  float height;
  if (!tracePrism(point, localNormal, height)) { return false; }
  PrismPatch gridPatch = prismPatches[prismPatchIndex];
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
  normal = normalize(transpose(inverse(mat3(gridPatch.current))) * localNormal);
  vec3 u, v;
  prismSurfaceAxes(localNormal, u, v);
#if SURFACE_BASE_LOCATION == 0
  prismShadowReceiver = prismReceiverPlane(mat3(prismView.lightFromWorld),
                                          mat3(gridPatch.current), localNormal);
#endif
  vec3 localDx, localDy;
  vec2 pixel = (gl_FragCoord.xy - prismViewport.area.xy) / prismViewport.area.zw;
  vec2 ndc = vec2(pixel.x * 2.0 - 1.0, 1.0 - pixel.y * 2.0);
  prismPositionDifferentials(prismLocalFromClip, localNormal, height, u, v, ndc,
                             prismViewport.area.zw, localDx, localDy);
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
