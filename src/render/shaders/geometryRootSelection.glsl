bool geometryRootVisible(GeometryRoot root) {
  if ((root.instance.w & kGeometryShadowOnly) != 0u) { return false; }
  if ((root.instance.w & kGeometryRootBound) == 0u) { return true; }
  const mat4 model = placements[root.instance.x].current;
  float widest;
  vec4 worldSphere;
  if (!cullAffineScale(model, widest) ||
      !cullAffineSphere(model, root.localSphere, view.shift.xyz, widest, worldSphere)) {
    return true;
  }
  for (uint at = 0u; at < 6u; ++at) {
    const vec4 plane = view.planes[at];
    if (dot(plane.xyz, plane.xyz) >= 0.5 &&
        cullAffineOutside(plane, worldSphere.xyz, worldSphere.w)) { return false; }
  }
  return true;
}
