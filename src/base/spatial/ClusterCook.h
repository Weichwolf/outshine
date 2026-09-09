#ifndef OUTSHINE_BASE_SPATIAL_CLUSTERCOOK_H
#define OUTSHINE_BASE_SPATIAL_CLUSTERCOOK_H

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>
#include "ClusterDag.h"

namespace outshine {

struct ClusteredMesh {
  std::vector<DagCluster> Clusters;
  std::vector<uint32_t> Index;
};

struct ClusterMeshInput {
  std::span<const float> PositionsM;
  std::span<const uint32_t> Indices;
  uint32_t StrideFloats = 3;
};

enum class ClusterError {
  InvalidLayout,
  InvalidIndex,
  NonFinitePosition,
  InvalidLimit,
  CapacityExceeded,
  BoundsOverflow
};

[[nodiscard]] std::string_view Describe(ClusterError error) noexcept;

[[nodiscard]] std::expected<ClusteredMesh, ClusterError> CookClusters(ClusterMeshInput input,
                                                                      uint32_t triangleLimit);

}
#endif
