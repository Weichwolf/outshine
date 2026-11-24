#include "PrismPlacements.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
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

std::expected<uint32_t, PrismInstanceError> PackCoating(const std::optional<Vec4f> &coating) {
  if (!coating) { return 0u; }
  if (!std::ranges::all_of(*coating, [](float value) { return value >= 0.0f && value <= 1.0f; })) {
    return std::unexpected(PrismInstanceError::InvalidCoating);
  }
  constexpr auto bitsPerChannel = static_cast<unsigned>(std::numeric_limits<uint8_t>::digits);
  constexpr auto maxChannel = static_cast<float>(std::numeric_limits<uint8_t>::max());
  const auto byte = [&](size_t channel) {
    return static_cast<uint32_t>(std::lround((*coating)[channel] * maxChannel));
  };
  return byte(0) | (byte(1) << bitsPerChannel) | (byte(2) << (2u * bitsPerChannel)) |
         (std::max(1u, byte(3)) << (3u * bitsPerChannel));
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
    const auto coat = PackCoating(instance.TopCoating);
    if (!coat) { return std::unexpected(coat.error()); }
    records.push_back(
        {.Current = instance.Current,
         .Previous = instance.Previous,
         .Samples = {static_cast<uint32_t>(patch.FirstSample), patch.Columns, patch.Rows, 0},
         .Shape = {patch.PitchX, patch.PitchZ, patch.FillFraction, bounds.Lower},
         .Bounds = {bounds.Upper, bounds.Width, bounds.Depth},
         .TopCoating = *coat});
  }
  return records;
}

std::expected<ConvexPrismUpload, PrismInstanceError>
PackConvexPrisms(std::span<const ConvexPrism> shapes) {
  constexpr size_t spansPerShape = 18;
  if (shapes.size() > std::numeric_limits<uint32_t>::max() / spansPerShape) {
    return std::unexpected(PrismInstanceError::InstanceCount);
  }
  const auto finite = [](float value) { return std::isfinite(value); };
  ConvexPrismUpload upload;
  upload.Samples.reserve(shapes.size() * spansPerShape);
  upload.Patches.reserve(shapes.size());
  for (const auto &shape : shapes) {
    if (const auto valid = CheckPlacement(shape.SceneFromLocal); !valid) {
      return std::unexpected(valid.error());
    }
    const auto &low = shape.Bounds.Min;
    const auto &high = shape.Bounds.Max;
    if (!shape.Bounds.Holds(high) || !std::ranges::all_of(low, finite) ||
        !std::ranges::all_of(high, finite) || !std::ranges::all_of(shape.Bounds.Span(), finite) ||
        !std::ranges::all_of(shape.Primary, finite) ||
        !std::ranges::all_of(shape.Secondary, finite)) {
      return std::unexpected(PrismInstanceError::InvalidConvexPrism);
    }
    upload.Patches.push_back(
        {.Current = shape.SceneFromLocal,
         .Previous = shape.SceneFromLocal,
         .Samples = {static_cast<uint32_t>(upload.Samples.size()), shape.SecondaryFaces, 0, 1},
         .Shape = {low[0], low[1], low[2], 0},
         .Bounds = {high[1], high[0], high[2]}});
    for (const auto &plane : shape.Planes) {
      if (!std::ranges::all_of(plane, finite) ||
          (plane[0] == 0 && plane[1] == 0 && plane[2] == 0 && plane[3] < 0)) {
        return std::unexpected(PrismInstanceError::InvalidConvexPrism);
      }
      upload.Samples.push_back({.Lower = plane[0], .Upper = plane[1]});
      upload.Samples.push_back({.Lower = plane[2], .Upper = plane[3]});
    }
    for (const auto &tint : {shape.Primary, shape.Secondary}) {
      upload.Samples.push_back({.Lower = tint[0], .Upper = tint[1]});
      upload.Samples.push_back({.Lower = tint[2], .Upper = tint[3]});
    }
  }
  return upload;
}
}
