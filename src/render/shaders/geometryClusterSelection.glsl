#include "depthRectangle.glsl"
#include "cullAffine.glsl"
const uint kClusterFloat4s = 3u;
float projectedError(float error, vec3 centre, float radius) {
  float away = max(length(centre) - radius, 1.0e-3);
  return error * view.errorPerMetre / away;
}

bool oppositeCone(mat4 model, vec4 sphere, vec2 cone, vec3 relativeCentre, float widest) {
  vec3 perspective = vec3(view.clip[0].w, view.clip[1].w, view.clip[2].w);
  if (cone.y <= 0.0 || widest > 1.01 || all(equal(perspective, vec3(0.0)))) { return false; }
  mat3 linear = mat3(model);
  float det = determinant(linear);
  if (det < 0.99 || det > 1.01) { return false; }
  vec3 dir = inverse(linear) * -relativeCentre;
  uint packed = uint(cone.x);
  uvec2 qr = uvec2(packed >> 12u, packed & 4095u);
  vec2 fold = vec2(qr) * (2.0 / 4095.0) - 1.0;
  vec3 axis = vec3(fold, 1.0 - abs(fold.x) - abs(fold.y));
  if (axis.z < 0.0) {
    axis.xy = (1.0 - abs(axis.yx)) *
              mix(vec2(-1.0), vec2(1.0), greaterThanEqual(axis.xy, vec2(0.0)));
  }
  axis = normalize(axis);
  float along = dot(dir, axis);
  if (along >= 0.0) { return false; }
  float lateral = length(cross(dir, axis));
  float sine = sqrt(max(0.0, 1.0 - cone.y * cone.y));
  float guard = 4.0e-5 * (length(dir) + sphere.w + length(model[3].xyz) + length(view.shift.xyz) + 1.0);
  return along * cone.y + lateral * sine + sphere.w + guard < 0.0;
}

uint geometryClusterCount(mat4 model, uint sphereIndex, uint indexCount) {
  const vec4 sphere = spheres[sphereIndex * kClusterFloat4s];
  const vec4 parent = spheres[sphereIndex * kClusterFloat4s + 1u];
  const vec4 errs = spheres[sphereIndex * kClusterFloat4s + 2u];
  float widest;
  vec4 worldSphere;
  if (!cullAffineScale(model, widest) ||
      !cullAffineSphere(model, sphere, view.shift.xyz, widest, worldSphere)) {
    return indexCount;
  }
  const vec3 centre = worldSphere.xyz;
  const float radius = worldSphere.w;
  if (oppositeCone(model, sphere, errs.zw, centre, widest)) { return 0u; }

  for (uint at = 0u; at < 6u; ++at) {
    const vec4 plane = view.planes[at];
    if (dot(plane.xyz, plane.xyz) < 0.5) { continue; }
    if (cullAffineOutside(plane, centre, radius)) {
      return 0u;
    }
  }

  if (view.errorPerMetre > 0.0) {
    vec4 parentSphere;
    if (!cullAffineSphere(model, parent, view.shift.xyz, widest, parentSphere)) {
      return indexCount;
    }
    const float mine = projectedError(errs.x * widest, centre, radius);
    const float theirs = projectedError(errs.y * widest, parentSphere.xyz, parentSphere.w);
    if (!(mine <= 1.0 && theirs > 1.0)) {
      return 0u;
    }
  }
  if (view.occludes != 0u && view.sourceWide != 0u && view.sourceHigh != 0u) {
    const vec4 said = view.clip * vec4(centre, 1.0);
    const vec3 xrow = vec3(view.clip[0][0], view.clip[1][0], view.clip[2][0]);
    const vec3 yrow = vec3(view.clip[0][1], view.clip[1][1], view.clip[2][1]);
    const vec3 zrow = vec3(view.clip[0][2], view.clip[1][2], view.clip[2][2]);
    const vec3 wrow = vec3(view.clip[0][3], view.clip[1][3], view.clip[2][3]);
    const float xradius = radius * length(xrow);
    const float yradius = radius * length(yrow);
    const float zradius = radius * length(zrow);
    const float wradius = radius * length(wrow);
    const float wleast = said.w - wradius;
    if (wleast > 1.0e-6) {
      const float wmost = said.w + wradius;
      const float xleast = said.x - xradius;
      const float xmost = said.x + xradius;
      const float yleast = said.y - yradius;
      const float ymost = said.y + yradius;
      const float zleast = said.z - zradius;
      const float zmost = said.z + zradius;
      const vec2 least = vec2(min(min(xleast / wleast, xleast / wmost),
                                  min(xmost / wleast, xmost / wmost)),
                              min(min(yleast / wleast, yleast / wmost),
                                  min(ymost / wleast, ymost / wmost)));
      const vec2 most = vec2(max(max(xleast / wleast, xleast / wmost),
                                 max(xmost / wleast, xmost / wmost)),
                             max(max(yleast / wleast, yleast / wmost),
                                 max(ymost / wleast, ymost / wmost)));
      const float nearest = max(max(zleast / wleast, zleast / wmost),
                                max(zmost / wleast, zmost / wmost));
      const vec2 lo = clamp(vec2(least.x * 0.5 + 0.5, 0.5 - most.y * 0.5), 0.0, 1.0);
      const vec2 hi = clamp(vec2(most.x * 0.5 + 0.5, 0.5 - least.y * 0.5), 0.0, 1.0);
      const vec2 source = vec2(view.sourceWide, view.sourceHigh);
      if (depthRectangleOccluded(lo, hi, nearest, source,
                                    view.pyramidWide, view.pyramidHigh, view.pyramidAt)) {
        return 0u;
      }

    }
  }
  return indexCount;
}
