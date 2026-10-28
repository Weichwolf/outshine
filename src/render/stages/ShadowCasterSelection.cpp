#include "ShadowCasterSelection.h"
#include "Mat4.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <limits>
#include <span>
#include <vector>

namespace outshine::Render {
namespace {
constexpr size_t kSphereFloats = 12;

struct SourceRange {
  uint32_t First;
  uint32_t Count;
};

std::optional<BatchSphere> WorldBounds(std::span<const float> spheres,
                                       uint32_t which,
                                       const std::optional<SphereTransform> &transform) {
  if (!transform) { return std::nullopt; }
  const size_t at = size_t{which} * kSphereFloats;
  if (at > spheres.size() || spheres.size() - at < kSphereFloats) { return std::nullopt; }
  BatchSphere local{};
  std::ranges::copy(spheres.subspan(at, local.size()), local.begin());
  if (local[3] < 0 || !std::ranges::all_of(local, NormalOrZero)) { return std::nullopt; }
  return transform->Apply(local);
}

bool Append(std::vector<ShadowCasterRange> &into,
            SourceRange source,
            std::span<const GeometryClusterTemplate> templates,
            std::span<const float> spheres,
            const std::optional<SphereTransform> &transform) {
  const uint64_t end = uint64_t{source.First} + source.Count;
  uint64_t covered = source.First;
  uint32_t previous = source.First;
  for (const auto &one : templates) {
    const uint32_t first = one.RelativeFirstIndex;
    const uint64_t next = uint64_t{first} + one.IndexCount;
    if (first < previous || first > covered || next > end || one.IndexCount == 0 ||
        one.IndexCount % 3 != 0 || (first - source.First) % 3 != 0) {
      return false;
    }
    into.push_back({.First = first,
                    .Count = one.IndexCount,
                    .Bounds = WorldBounds(spheres, one.SphereIndex, transform)});
    covered = std::max(covered, next);
    previous = first;
  }
  return covered == end;
}
}

void ShadowCasterSelection::Invalidate() noexcept {
  Ready_ = false;
  Source_ = {};
}

void ShadowCasterSelection::Prepare(std::span<const DrawBatch> batches,
                                    std::span<const GeometryClusterTemplate> templates,
                                    std::span<const float> spheres,
                                    std::span<const GpuPlacement> placements,
                                    uint32_t mainMeshRows,
                                    bool mainMeshBoundsCurrent) {
  Source_ = {.Batches = batches,
             .Templates = templates,
             .Spheres = spheres,
             .Placements = placements,
             .MainMeshRows = mainMeshRows,
             .MainMeshBoundsCurrent = mainMeshBoundsCurrent};
  for (const auto &batch : batches) {
    if (batch.FirstJob > templates.size() || batch.JobCount > templates.size() - batch.FirstJob) {
      continue;
    }
    if (batch.JobCount >
        std::numeric_limits<size_t>::max() / sizeof(ShadowCasterRange) - Source_.RangeCapacity) {
      Invalidate();
      return;
    }
    Source_.RangeCapacity += batch.JobCount;
  }
  Batches_.assign(batches.size(), {});
  Ranges_.clear();
  Ready_ = true;
}

std::span<const ShadowCasterRange> ShadowCasterSelection::Ranges(size_t batch) {
  if (!Ready_ || batch >= Batches_.size()) { return {}; }
  auto &held = Batches_[batch];
  if (held.Resolved) { return std::span(Ranges_).subspan(held.First, held.Count); }
  held.Resolved = true;
  const auto &source = Source_.Batches[batch];
  if ((source.ModelSlot < Source_.MainMeshRows && !Source_.MainMeshBoundsCurrent) ||
      source.JobCount == 0 || source.Instances != 1 ||
      source.ModelSlot >= Source_.Placements.size() || source.FirstJob > Source_.Templates.size() ||
      source.JobCount > Source_.Templates.size() - source.FirstJob) {
    return {};
  }
  const auto templates = Source_.Templates.subspan(source.FirstJob, source.JobCount);
  const size_t first = Ranges_.size();
  Ranges_.reserve(Source_.RangeCapacity);
  Mat4 model{};
  std::ranges::copy(Source_.Placements[source.ModelSlot].Current, model.begin());
  if (!Append(Ranges_,
              {.First = source.FirstIndex, .Count = source.IndexCount},
              templates,
              Source_.Spheres,
              SphereTransform::From(model))) {
    Ranges_.resize(first);
    return {};
  }
  held.First = first;
  held.Count = static_cast<uint32_t>(Ranges_.size() - first);
  return std::span(Ranges_).subspan(held.First, held.Count);
}

size_t ShadowCasterSelection::CachedRanges() const noexcept {
  return Ready_ ? Ranges_.size() : 0;
}

size_t ShadowCasterSelection::HeapBytes() const noexcept {
  return Batches_.capacity() * sizeof(BatchRange) + Ranges_.capacity() * sizeof(ShadowCasterRange);
}
}
