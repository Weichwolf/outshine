#include "PrismPlacements.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <vector>

namespace outshine::Render {
namespace {
std::expected<void, PrismInstanceError> CheckPlacement(const Mat4f &matrix) noexcept {
  if (!std::ranges::all_of(matrix, [](float value) { return std::isfinite(value); })) {
    return std::unexpected(PrismInstanceError::NonfinitePlacement);
  }
  if (matrix[3] != 0 || matrix[7] != 0 || matrix[11] != 0 || matrix[15] != 1) {
    return std::unexpected(PrismInstanceError::NonAffinePlacement);
  }
  const float determinant = matrix[0] * (matrix[5] * matrix[10] - matrix[9] * matrix[6]) -
                            matrix[4] * (matrix[1] * matrix[10] - matrix[9] * matrix[2]) +
                            matrix[8] * (matrix[1] * matrix[6] - matrix[5] * matrix[2]);
  if (!std::isfinite(determinant) || determinant == 0) {
    return std::unexpected(PrismInstanceError::SingularPlacement);
  }
  return {};
}
}

std::expected<std::vector<GpuPrismPatch>, PrismInstanceError> PackPrismPlacements(
    const PrismGrid &grid, const PrismGridInfo &info, std::span<const PrismInstance> instances) {
  if (grid.Patches.size() != info.Patches.size()) {
    return std::unexpected(PrismInstanceError::PatchRange);
  }
  if (instances.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(PrismInstanceError::InstanceCount);
  }
  std::vector<GpuPrismPatch> records;
  records.reserve(instances.size());
  for (const auto &instance : instances) {
    if (instance.Patch >= grid.Patches.size()) {
      return std::unexpected(PrismInstanceError::PatchRange);
    }
    if (const auto valid = CheckPlacement(instance.Current); !valid) {
      return std::unexpected(valid.error());
    }
    if (const auto valid = CheckPlacement(instance.Previous); !valid) {
      return std::unexpected(valid.error());
    }
    const auto &patch = grid.Patches[instance.Patch];
    const auto &bounds = info.Patches[instance.Patch];
    constexpr uint64_t sampleMost = uint64_t{std::numeric_limits<uint32_t>::max()} + 1;
    if (patch.FirstSample >= sampleMost || bounds.Samples > sampleMost - patch.FirstSample) {
      return std::unexpected(PrismInstanceError::SampleRange);
    }
    if (bounds.Occupied == 0) { continue; }
    records.push_back(
        {.Current = instance.Current,
         .Previous = instance.Previous,
         .Samples = {static_cast<uint32_t>(patch.FirstSample), patch.Columns, patch.Rows, 0},
         .Shape = {patch.PitchX, patch.PitchZ, patch.FillFraction, bounds.Lower},
         .Bounds = {bounds.Upper, bounds.Width, bounds.Depth, 0}});
  }
  return records;
}
}
