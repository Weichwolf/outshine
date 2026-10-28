#ifndef OUTSHINE_RENDER_STAGES_SHADOWCASTERSELECTION_H
#define OUTSHINE_RENDER_STAGES_SHADOWCASTERSELECTION_H

#include "BatchVisibility.h"
#include "DrawList.h"
#include "GpuPlacement.h"
#include "GeometryHierarchy.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Render {
struct ShadowCasterRange {
  uint32_t First = 0;
  uint32_t Count = 0;
  std::optional<BatchSphere> Bounds;
};

class ShadowCasterSelection {
public:
  void Invalidate() noexcept;

  [[nodiscard]] bool Ready() const noexcept { return Ready_; }

  void Prepare(std::span<const DrawBatch> batches,
               std::span<const GeometryClusterTemplate> templates,
               std::span<const float> spheres,
               std::span<const GpuPlacement> placements,
               uint32_t mainMeshRows,
               bool mainMeshBoundsCurrent);
  [[nodiscard]] std::span<const ShadowCasterRange> Ranges(size_t batch);
  [[nodiscard]] size_t CachedRanges() const noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct BatchRange {
    size_t First = 0;
    uint32_t Count = 0;
    bool Resolved = false;
  };

  struct Source {
    std::span<const DrawBatch> Batches;
    std::span<const GeometryClusterTemplate> Templates;
    size_t RangeCapacity = 0;
    std::span<const float> Spheres;
    std::span<const GpuPlacement> Placements;
    uint32_t MainMeshRows = 0;
    bool MainMeshBoundsCurrent = false;
  };

  bool Ready_ = false;
  Source Source_;
  std::vector<BatchRange> Batches_;
  std::vector<ShadowCasterRange> Ranges_;
};
}
#endif
