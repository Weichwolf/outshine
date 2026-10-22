#ifndef OUTSHINE_RENDER_PRISM_PRISMGRID_H
#define OUTSHINE_RENDER_PRISM_PRISMGRID_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <scene/PrismGrid.h>
#include <span>
#include <vector>

namespace outshine::Render {
enum class PrismGridError : uint8_t {
  InvalidShape,
  InvalidPitch,
  InvalidFill,
  MissingSamples,
  SampleRange,
  NonfiniteSample,
  ExtentOverflow,
  CountOverflow
};

struct PrismPatchInfo {
  size_t Samples = 0;
  size_t Occupied = 0;
  float Width = 0;
  float Depth = 0;
  float Lower = 0;
  float Upper = 0;
};

struct PrismGridInfo {
  size_t SampleBytes = 0;
  uint64_t ReferencedSamples = 0;
  uint64_t Occupied = 0;
  std::vector<PrismPatchInfo> Patches;
};

[[nodiscard]] std::expected<PrismPatchInfo, PrismGridError>
InspectPrismPatch(const PrismPatch &patch, std::span<const PrismSpan> samples) noexcept;

[[nodiscard]] std::expected<PrismGridInfo, PrismGridError> InspectPrismGrid(const PrismGrid &grid);
}
#endif
