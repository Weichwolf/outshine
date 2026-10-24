#include "prismView.glsl"
#include "prismTrace.glsl"

bool readPrismSurface() {
  vec3 point, localNormal;
  if (!tracePrism(point, localNormal)) { return false; }
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
  if (localNormal.x != 0.0) {
    u = vec3(0.0, 0.0, -localNormal.x);
    v = vec3(0.0, 1.0, 0.0);
  } else if (localNormal.y != 0.0) {
    u = vec3(1.0, 0.0, 0.0);
    v = vec3(0.0, 0.0, -localNormal.y);
  } else {
    u = vec3(localNormal.z, 0.0, 0.0);
    v = vec3(0.0, 1.0, 0.0);
  }
#if SUBJECT_BASE_LOCATION == 0
  prismShadowReceiver = prismReceiverPlane(mat3(prismView.lightFromWorld),
                                          mat3(gridPatch.current), localNormal);
#endif
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
#if SUBJECT_WRITES_VELOCITY
  curClip = clip;
  vec4 previousWorld = gridPatch.previous * vec4(point, 1.0) + vec4(prismView.prevShift.xyz, 0.0);
  prevClip = prismView.prevViewProj * previousWorld;
#endif
  return true;
}
