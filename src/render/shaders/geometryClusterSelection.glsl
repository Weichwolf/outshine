#include "depthRectangle.glsl"
#include "cullAffine.glsl"
const uint kClusterFloat4s = 3u;
float projectedError(float error, vec3 centre, float radius) {
  float away = max(length(centre) - radius, 1.0e-3);
  return error * view.errorPerMetre / away;
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
