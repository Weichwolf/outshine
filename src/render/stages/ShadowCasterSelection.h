#ifndef OUTSHINE_RENDER_STAGES_SHADOWCASTERSELECTION_H
#define OUTSHINE_RENDER_STAGES_SHADOWCASTERSELECTION_H

#include "BatchVisibility.h"
#include "DrawList.h"
#include "GpuPlacement.h"
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
  void Invalidate() noexcept { Ready_ = false; }

  [[nodiscard]] bool Ready() const noexcept { return Ready_; }

  void Prepare(std::span<const DrawBatch> batches,
               std::span<const uint32_t> jobs,
               std::span<const float> spheres,
               std::span<const GpuPlacement> placements,
               uint32_t subjectRows,
               bool subjectBoundsCurrent);
  [[nodiscard]] std::span<const ShadowCasterRange> Ranges(size_t batch) const noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct BatchRange {
    size_t First = 0;
    size_t Count = 0;
  };

  bool Ready_ = false;
  std::vector<BatchRange> Batches_;
  std::vector<ShadowCasterRange> Ranges_;
};
}
#endif
