#ifndef OUTSHINE_RENDER_SCENE_GEOMETRYSNAPSHOT_H
#define OUTSHINE_RENDER_SCENE_GEOMETRYSNAPSHOT_H

#include "GeometryHierarchy.h"
#include <array>
#include <cstdint>
#include <vector>

namespace outshine::Render {
struct GeometrySnapshot {
  uint64_t Generation = 0;
  uint64_t ReadbackBytes = 0;
  bool SelectionReady = false;
  std::vector<GeometryRoot> Roots;
  std::vector<GeometryClusterTemplate> Templates;
  std::vector<std::array<uint32_t, 12>> Batches;
  std::vector<uint32_t> SourceIndices;
  std::vector<uint32_t> Kept;
  std::vector<uint32_t> Visible;
  std::vector<uint32_t> Arguments;
};
}
#endif
