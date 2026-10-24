#include "ShadowCasterSelection.h"
#include "Mat4.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Render {
namespace {
constexpr size_t kSphereFloats = 12;

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
            const DrawBatch &batch,
            size_t batchIndex,
            std::span<const uint32_t> jobs,
            std::span<const float> spheres,
            std::span<const GpuPlacement> placements) {
  if (batch.JobCount == 0 || batch.Instances != 1 || batch.ModelSlot >= placements.size() ||
      batch.FirstJob > jobs.size() / DrawList::kJobWords ||
      batch.JobCount > jobs.size() / DrawList::kJobWords - batch.FirstJob) {
    return false;
  }
  Mat4 model{};
  std::ranges::copy(placements[batch.ModelSlot].Current, model.begin());
  const auto transform = SphereTransform::From(model);
  const uint64_t end = uint64_t{batch.FirstIndex} + batch.IndexCount;
  uint64_t covered = batch.FirstIndex;
  uint32_t previous = batch.FirstIndex;
  for (size_t at = batch.FirstJob; at < size_t{batch.FirstJob} + batch.JobCount; ++at) {
    const auto job = jobs.subspan(at * DrawList::kJobWords, DrawList::kJobWords);
    const uint64_t next = uint64_t{job[2]} + job[3];
    if (job[1] != batchIndex || job[2] < previous || job[2] > covered || next > end ||
        job[3] == 0 || job[3] % 3 != 0 || (job[2] - batch.FirstIndex) % 3 != 0) {
      return false;
    }
    into.push_back(
        {.First = job[2], .Count = job[3], .Bounds = WorldBounds(spheres, job[0], transform)});
    covered = std::max(covered, next);
    previous = job[2];
  }
  return covered == end;
}
}

void ShadowCasterSelection::Prepare(std::span<const DrawBatch> batches,
                                    std::span<const uint32_t> jobs,
                                    std::span<const float> spheres,
                                    std::span<const GpuPlacement> placements,
                                    uint32_t subjectRows,
                                    bool subjectBoundsCurrent) {
  Batches_.assign(batches.size(), {});
  Ranges_.clear();
  Ranges_.reserve(jobs.size() / DrawList::kJobWords);
  for (size_t at = 0; at < batches.size(); ++at) {
    const auto &batch = batches[at];
    if (batch.ModelSlot < subjectRows && !subjectBoundsCurrent) { continue; }
    const size_t first = Ranges_.size();
    if (!Append(Ranges_, batch, at, jobs, spheres, placements)) {
      Ranges_.resize(first);
      continue;
    }
    Batches_[at] = {.First = first, .Count = Ranges_.size() - first};
  }
  Ready_ = true;
}

std::span<const ShadowCasterRange> ShadowCasterSelection::Ranges(size_t batch) const noexcept {
  if (!Ready_ || batch >= Batches_.size()) { return {}; }
  const auto range = Batches_[batch];
  return std::span(Ranges_).subspan(range.First, range.Count);
}

size_t ShadowCasterSelection::HeapBytes() const noexcept {
  return Batches_.capacity() * sizeof(BatchRange) + Ranges_.capacity() * sizeof(ShadowCasterRange);
}
}
