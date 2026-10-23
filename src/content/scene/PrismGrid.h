#ifndef OUTSHINE_CONTENT_SCENE_PRISMGRID_H
#define OUTSHINE_CONTENT_SCENE_PRISMGRID_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <vector>

namespace outshine {
struct alignas(8) PrismSpan {
  float Lower = 0;
  float Upper = 0;
};

static_assert(sizeof(PrismSpan) == 8 && alignof(PrismSpan) == 8);
static_assert(offsetof(PrismSpan, Lower) == 0 && offsetof(PrismSpan, Upper) == 4);

struct PrismPatch {
  uint64_t FirstSample = 0;
  uint32_t Columns = 0;
  uint32_t Rows = 0;
  float PitchX = 1;
  float PitchZ = 1;
  float FillFraction = 1;
};

struct PrismGrid {
  std::shared_ptr<const std::vector<PrismSpan>> Samples;
  std::vector<PrismPatch> Patches;
};

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
