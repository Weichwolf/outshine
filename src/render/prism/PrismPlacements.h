#ifndef OUTSHINE_RENDER_PRISM_PRISMPLACEMENTS_H
#define OUTSHINE_RENDER_PRISM_PRISMPLACEMENTS_H

#include "GpuPrismPatch.h"
#include "ConvexPrism.h"
#include <PrismGrid.h>
#include "PrismInstance.h"
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Render {
enum class PrismInstanceError : uint8_t {
  PatchRange,
  SampleRange,
  NonfinitePlacement,
  NonAffinePlacement,
  SingularPlacement,
  InvalidCoating,
  InstanceCount,
  InvalidConvexPrism
};

[[nodiscard]] std::expected<std::vector<GpuPrismPatch>, PrismInstanceError> PackPrismPlacements(
    const PrismGrid &grid, const PrismGridInfo &info, std::span<const PrismInstance> instances);

struct ConvexPrismUpload {
  std::vector<PrismSpan> Samples;
  std::vector<GpuPrismPatch> Patches;
};

[[nodiscard]] std::expected<ConvexPrismUpload, PrismInstanceError>
PackConvexPrisms(std::span<const ConvexPrism> shapes);
}
#endif
