struct GeometryRoot {
  vec4 localSphere;
  uvec4 cluster;
  uvec4 instance;
};
struct GeometryQueueState {
  uvec4 counts;
  uvec4 dispatch;
};
#ifndef GEOMETRY_HIERARCHY_BINDING
#define GEOMETRY_HIERARCHY_BINDING 1
#endif
layout(std140, set = 2, binding = GEOMETRY_HIERARCHY_BINDING) uniform GeometryHierarchyView {
  uvec4 counts;
  uvec4 work;
} hierarchy;
const uint kGeometryIndexCold = 2u;
const uint kGeometryChunkClusters = 64u;
const uint kGeometryRootBound = 1u;
const uint kGeometryUnboundedDeformation = 2u;
uint geometryGroup() {
  return gl_WorkGroupID.y * hierarchy.work.y + gl_WorkGroupID.x;
}
uint geometryCeil(uint value, uint divisor) {
  return value / divisor + uint(value % divisor != 0u);
}
