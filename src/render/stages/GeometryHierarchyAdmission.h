#ifndef OUTSHINE_RENDER_STAGES_GEOMETRYHIERARCHYADMISSION_H
#define OUTSHINE_RENDER_STAGES_GEOMETRYHIERARCHYADMISSION_H

#include "GeometryHierarchy.h"
#include "native/Device.h"
#include <cstdint>
#include <span>
#include <vector>

namespace outshine::Render {

struct GeometryHierarchyAdmission {
  GeometryHierarchyView View;
  Native::Workgroups RootGroups;
  Native::Workgroups ScanGroups;
  uint32_t IndexWords = 0;
  uint64_t TemplateVisits = 0;
  std::vector<uint32_t> ChunkPrefix;
};

struct GeometryHierarchyInputs {
  std::span<const GeometryRoot> Roots;
  std::span<const GeometryClusterTemplate> Templates;
  uint32_t SphereCount = 0;
  uint32_t PlacementCount = 0;
  uint32_t SourceIndexCount = 0;
  uint32_t BatchCount = 0;
};

[[nodiscard]] Native::Result<GeometryHierarchyAdmission>
AdmitGeometryHierarchy(const GeometryHierarchyInputs &inputs, const Native::Capabilities &limits);

}
#endif
