#ifndef OUTSHINE_BASE_SPATIAL_CLUSTERCOOK_H
#define OUTSHINE_BASE_SPATIAL_CLUSTERCOOK_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>
#include "ClusterDag.h"
#include "math/Vec3.h"

namespace outshine {

struct ClusteredMesh {
  std::vector<DagCluster> Clusters;
  std::vector<uint32_t> Index;

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return Clusters.capacity() * sizeof(DagCluster) + Index.capacity() * sizeof(uint32_t);
  }
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

class ClusterCookJob {
public:
  explicit ClusterCookJob(ClusterMeshInput input, uint32_t triangleLimit) noexcept;

  [[nodiscard]] std::expected<bool, ClusterError> Advance(size_t itemsMost);

  [[nodiscard]] std::expected<bool, ClusterError> AdvanceWithin(size_t &itemsLeft);

  [[nodiscard]] bool Ready() const noexcept;

  [[nodiscard]] ClusteredMesh Take() noexcept;

private:
  struct Bounds {
    std::array<double, 3> Low{};
    std::array<double, 3> High{};
  };

  enum class Stage {
    Layout,
    Positions,
    Indices,
    Order,
    SortClear,
    SortCount,
    SortPrefix,
    SortScatter,
    Pack,
    Complete,
    Failed
  };

  enum class Flow { Continue, Yield, Complete };

  static void Clear(Bounds &bounds) noexcept;
  static void Include(Bounds &bounds, const Vec3 &point) noexcept;
  [[nodiscard]] ClusterError Fail(ClusterError error) noexcept;
  [[nodiscard]] std::expected<Flow, ClusterError> AdvanceStage(size_t &itemsLeft);
  [[nodiscard]] std::expected<Flow, ClusterError> AdvancesLayout();
  [[nodiscard]] std::expected<Flow, ClusterError> AdvancesPositions(size_t &itemsLeft);
  [[nodiscard]] std::expected<Flow, ClusterError> AdvancesIndices(size_t &itemsLeft);
  [[nodiscard]] std::expected<Flow, ClusterError> AdvancesOrder(size_t &itemsLeft);
  [[nodiscard]] std::expected<Flow, ClusterError> AdvancesSortClear(size_t &itemsLeft);
  [[nodiscard]] std::expected<Flow, ClusterError> AdvancesSortCount(size_t &itemsLeft);
  [[nodiscard]] std::expected<Flow, ClusterError> AdvancesSortPrefix(size_t &itemsLeft);
  [[nodiscard]] std::expected<Flow, ClusterError> AdvancesSortScatter(size_t &itemsLeft);
  void BeginsPack();
  [[nodiscard]] std::expected<Flow, ClusterError> AdvancesPack(size_t &itemsLeft);

  ClusterMeshInput Input_;
  uint32_t TriangleLimit_ = 0;
  Stage Stage_ = Stage::Layout;
  ClusterError Failure_ = ClusterError::InvalidLayout;
  Bounds InputBounds_;
  Bounds ClusterBounds_;
  std::array<size_t, 1024> Counts_{};
  std::array<size_t, 1024> Offsets_{};
  std::vector<uint64_t> Order_;
  std::vector<uint64_t> Sorting_;
  ClusteredMesh Result_;
  size_t Cursor_ = 0;
  size_t Prefix_ = 0;
  size_t Running_ = 0;
  size_t ClusterFirst_ = 0;
  size_t ClusterTriangles_ = 0;
  unsigned Pass_ = 0;
};

[[nodiscard]] std::expected<ClusteredMesh, ClusterError> CookClusters(ClusterMeshInput input,
                                                                      uint32_t triangleLimit);

}
#endif
