#ifndef OUTSHINE_BASE_SPATIAL_CLUSTERCOOK_H
#define OUTSHINE_BASE_SPATIAL_CLUSTERCOOK_H

#include <cstdint>
#include <span>
#include <vector>

#include "ClusterDag.h"

namespace outshine {

struct Cooked {
  std::vector<DagCluster> Clusters;
  std::vector<uint32_t> Index;

  std::vector<float> PositionsM;
  uint32_t FirstOwnVertex = 0;
};

[[nodiscard]] Cooked CookClusters(std::span<const float> positionsM,
                                  std::span<const uint32_t> indices,
                                  uint32_t mostTriangles,
                                  int strideFloats = 3);

struct Limits {
  uint32_t MostTriangles = 0;
  uint32_t MostLevels = 0;
};

[[nodiscard]] Cooked
CookDag(std::span<const float> positionsM, std::span<const uint32_t> indices, Limits within);

} // namespace outshine
#endif
