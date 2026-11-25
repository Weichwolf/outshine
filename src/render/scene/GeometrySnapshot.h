#ifndef OUTSHINE_RENDER_SCENE_GEOMETRYSNAPSHOT_H
#define OUTSHINE_RENDER_SCENE_GEOMETRYSNAPSHOT_H

#include "GeometryHierarchy.h"
#include "Vec3.h"
#include <array>
#include <cstdint>
#include <vector>

namespace outshine::Render {
struct GeometrySnapshot {
  uint64_t Generation = 0;
  uint64_t ReadbackBytes = 0;
  bool SelectionReady = false;
  Vec3 EyeFromAnchorM{};
  std::array<Vec3, 3> CameraAxes;
  std::array<float, 16> DepthClipFromEye{};
  std::array<double, 2> ViewportPx{};
  std::vector<GeometryRoot> Roots;
  std::vector<GeometryClusterTemplate> Templates;
  std::vector<std::array<float, 12>> Spheres;
  std::vector<std::array<float, 16>> Placements;
  std::vector<std::array<uint32_t, 12>> Batches;
  std::vector<uint32_t> SourceIndices;
  std::vector<uint32_t> Kept;
  std::vector<uint32_t> Visible;
  std::vector<uint32_t> Arguments;
  std::vector<float> Depth;
};
}
#endif
