#include "PrismGrid.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>

namespace outshine::Render {
std::expected<PrismPatchInfo, PrismGridError>
InspectPrismPatch(const PrismPatch &patch, std::span<const PrismSpan> samples) noexcept {
  if (patch.Columns == 0 || patch.Rows == 0) {
    return std::unexpected(PrismGridError::InvalidShape);
  }
  if (!std::isfinite(patch.PitchX) || !std::isfinite(patch.PitchZ) || patch.PitchX <= 0 ||
      patch.PitchZ <= 0) {
    return std::unexpected(PrismGridError::InvalidPitch);
  }
  if (!std::isfinite(patch.FillFraction) || patch.FillFraction <= 0 || patch.FillFraction > 1) {
    return std::unexpected(PrismGridError::InvalidFill);
  }
  const uint64_t count = uint64_t{patch.Columns} * patch.Rows;
  if (patch.FirstSample > samples.size() || count > samples.size() - patch.FirstSample) {
    return std::unexpected(PrismGridError::SampleRange);
  }
  const double width = static_cast<double>(patch.Columns) * patch.PitchX;
  const double depth = static_cast<double>(patch.Rows) * patch.PitchZ;
  if (width > std::numeric_limits<float>::max() || depth > std::numeric_limits<float>::max()) {
    return std::unexpected(PrismGridError::ExtentOverflow);
  }
  PrismPatchInfo result{.Samples = static_cast<size_t>(count),
                        .Width = static_cast<float>(width),
                        .Depth = static_cast<float>(depth),
                        .Lower = std::numeric_limits<float>::infinity(),
                        .Upper = -std::numeric_limits<float>::infinity()};
  if (result.Width < width) {
    result.Width = std::nextafter(result.Width, std::numeric_limits<float>::infinity());
  }
  if (result.Depth < depth) {
    result.Depth = std::nextafter(result.Depth, std::numeric_limits<float>::infinity());
  }
  for (const auto &cell : samples.subspan(static_cast<size_t>(patch.FirstSample), result.Samples)) {
    if (!std::isfinite(cell.Lower) || !std::isfinite(cell.Upper)) {
      return std::unexpected(PrismGridError::NonfiniteSample);
    }
    if (cell.Upper <= cell.Lower) { continue; }
    ++result.Occupied;
    result.Lower = std::min(result.Lower, cell.Lower);
    result.Upper = std::max(result.Upper, cell.Upper);
  }
  if (result.Occupied == 0) { result.Lower = result.Upper = 0; }
  if (static_cast<double>(result.Upper) - result.Lower > std::numeric_limits<float>::max()) {
    return std::unexpected(PrismGridError::ExtentOverflow);
  }
  return result;
}

std::expected<PrismGridInfo, PrismGridError> InspectPrismGrid(const PrismGrid &grid) {
  if (!grid.Samples) {
    if (grid.Patches.empty()) { return PrismGridInfo{}; }
    return std::unexpected(PrismGridError::MissingSamples);
  }
  if (grid.Samples->size() > std::numeric_limits<size_t>::max() / sizeof(PrismSpan)) {
    return std::unexpected(PrismGridError::CountOverflow);
  }
  PrismGridInfo result{.SampleBytes = grid.Samples->size() * sizeof(PrismSpan), .Patches = {}};
  result.Patches.reserve(grid.Patches.size());
  for (const auto &patch : grid.Patches) {
    auto info = InspectPrismPatch(patch, *grid.Samples);
    if (!info) { return std::unexpected(info.error()); }
    if (result.ReferencedSamples > std::numeric_limits<uint64_t>::max() - info->Samples ||
        result.Occupied > std::numeric_limits<uint64_t>::max() - info->Occupied) {
      return std::unexpected(PrismGridError::CountOverflow);
    }
    result.ReferencedSamples += info->Samples;
    result.Occupied += info->Occupied;
    result.Patches.push_back(*info);
  }
  return result;
}
}
