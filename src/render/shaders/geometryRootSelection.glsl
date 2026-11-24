uint geometryRootSelection(GeometryRoot root) {
  if ((root.instance.w & kGeometryShadowOnly) != 0u) { return kGeometryRejected; }
  if ((root.instance.w & kGeometryRootBound) == 0u) { return kGeometryPartial; }
  const mat4 model = placements[root.instance.x].current;
  float widest;
  vec4 worldSphere;
  if (!cullAffineScale(model, widest) ||
      !cullAffineSphere(model, root.localSphere, view.shift.xyz, widest, worldSphere)) {
    return kGeometryPartial;
  }
  bool direct = hierarchy.work.w != 0u && view.occludes == 0u &&
                (root.instance.w & kGeometryWholeRange) != 0u;
  for (uint at = 0u; at < 6u; ++at) {
    const vec4 plane = view.planes[at];
    if (dot(plane.xyz, plane.xyz) < 0.5) { continue; }
    if (cullAffineOutside(plane, worldSphere.xyz, worldSphere.w)) { return kGeometryRejected; }
    direct = direct && cullAffineOutside(-plane, worldSphere.xyz, worldSphere.w);
  }
  return direct ? kGeometryDirect : kGeometryPartial;
}
