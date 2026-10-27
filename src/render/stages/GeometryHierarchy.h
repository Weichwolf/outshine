#ifndef OUTSHINE_RENDER_STAGES_GEOMETRYHIERARCHY_H
#define OUTSHINE_RENDER_STAGES_GEOMETRYHIERARCHY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace outshine::Render {

inline constexpr size_t kGeometryRootBytes = 80;
inline constexpr uint32_t kGeometryChunkClusters = 64;
inline constexpr uint32_t kGeometryRootBound = 1;
inline constexpr uint32_t kGeometryUnboundedDeformation = 2;

struct alignas(16) GeometryRoot {
  std::array<float, 4> LocalSphere{};
  uint32_t TemplateFirst = 0;
  uint32_t TemplateCount = 0;
  uint32_t LogicalFirst = 0;
  uint32_t BatchRow = 0;
  uint32_t ModelSlot = 0;
  uint32_t IndexBase = 0;
  uint32_t IndexCapacity = 0;
  uint32_t Flags = 0;
  std::array<uint32_t, 4> Product{};
  std::array<uint32_t, 2> ResidencyRevision{};
  std::array<uint32_t, 2> Coverage{};
};

struct alignas(16) GeometryClusterTemplate {
  uint32_t SphereIndex = 0;
  uint32_t RelativeFirstIndex = 0;
  uint32_t IndexCount = 0;
  uint32_t Flags = 0;
};

struct alignas(16) GeometryQueueState {
  uint32_t Requested = 0;
  uint32_t Overflow = 0;
  uint32_t Visited = 0;
  uint32_t Accepted = 0;
  std::array<uint32_t, 3> Dispatch{0, 1, 1};
  uint32_t LogicalWork = 0;
};

struct alignas(16) GeometryHierarchyView {
  uint32_t RootCount = 0;
  uint32_t TemplateCount = 0;
  uint32_t ChunkCapacity = 0;
  uint32_t ClusterCapacity = 0;
  uint32_t WorstChunks = 0;
  uint32_t GridWidth = 0;
  std::array<uint32_t, 2> Reserved{};
};

static_assert(std::is_trivially_copyable_v<GeometryRoot>);
static_assert(sizeof(GeometryRoot) == kGeometryRootBytes && alignof(GeometryRoot) == 16);
static_assert(offsetof(GeometryRoot, LocalSphere) == 0);
static_assert(offsetof(GeometryRoot, TemplateFirst) == 16);
static_assert(offsetof(GeometryRoot, TemplateCount) == 16 + sizeof(uint32_t));
static_assert(offsetof(GeometryRoot, LogicalFirst) == 16 + 2 * sizeof(uint32_t));
static_assert(offsetof(GeometryRoot, BatchRow) == 16 + 3 * sizeof(uint32_t));
static_assert(offsetof(GeometryRoot, ModelSlot) == 32);
static_assert(offsetof(GeometryRoot, IndexBase) == 32 + sizeof(uint32_t));
static_assert(offsetof(GeometryRoot, IndexCapacity) == 32 + 2 * sizeof(uint32_t));
static_assert(offsetof(GeometryRoot, Flags) == 32 + 3 * sizeof(uint32_t));
static_assert(offsetof(GeometryRoot, Product) == size_t{3} * 16);
static_assert(offsetof(GeometryRoot, ResidencyRevision) == 64);
static_assert(offsetof(GeometryRoot, Coverage) == 64 + 2 * sizeof(uint32_t));
static_assert(sizeof(GeometryClusterTemplate) == 16 && alignof(GeometryClusterTemplate) == 16);
static_assert(offsetof(GeometryClusterTemplate, SphereIndex) == 0);
static_assert(offsetof(GeometryClusterTemplate, RelativeFirstIndex) == 4);
static_assert(offsetof(GeometryClusterTemplate, IndexCount) == 8);
static_assert(offsetof(GeometryClusterTemplate, Flags) == 12);
static_assert(sizeof(GeometryQueueState) == 32 && alignof(GeometryQueueState) == 16);
static_assert(offsetof(GeometryQueueState, Requested) == 0);
static_assert(offsetof(GeometryQueueState, Overflow) == 4);
static_assert(offsetof(GeometryQueueState, Visited) == 8);
static_assert(offsetof(GeometryQueueState, Accepted) == 12);
static_assert(offsetof(GeometryQueueState, Dispatch) == 16);
static_assert(offsetof(GeometryQueueState, LogicalWork) == 16 + 3 * sizeof(uint32_t));
static_assert(sizeof(GeometryHierarchyView) == 32 && alignof(GeometryHierarchyView) == 16);
static_assert(offsetof(GeometryHierarchyView, RootCount) == 0);
static_assert(offsetof(GeometryHierarchyView, TemplateCount) == 4);
static_assert(offsetof(GeometryHierarchyView, ChunkCapacity) == 8);
static_assert(offsetof(GeometryHierarchyView, ClusterCapacity) == 12);
static_assert(offsetof(GeometryHierarchyView, WorstChunks) == 16);
static_assert(offsetof(GeometryHierarchyView, GridWidth) == 16 + sizeof(uint32_t));
static_assert(offsetof(GeometryHierarchyView, Reserved) == 16 + 2 * sizeof(uint32_t));

}
#endif
