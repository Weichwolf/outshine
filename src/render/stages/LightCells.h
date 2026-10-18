#ifndef OUTSHINE_RENDER_STAGES_LIGHTCELLS_H
#define OUTSHINE_RENDER_STAGES_LIGHTCELLS_H

#include "SubjectLightingUniform.h"
#include "SubjectTypes.h"
#include "LightCellFrame.h"
#include "math/Vec3.h"
#include "math/Vec4.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace outshine::Render {
struct LightCellLimits {
  size_t EmitterBytes = 0;
  size_t IndexBytes = 0;
  size_t ScratchBytes = 0;
  uint64_t SphereBoxTests = 0;
};

struct LightCellCosts {
  uint64_t SphereBoxTests = 0;
  uint64_t Assignments = 0;
  size_t PackedBytes = 0;
  size_t ScratchBytes = 0;
  size_t Allocations = 0;
  size_t ScratchCopyBytes = 0;
};

struct LightCells {
  static constexpr uint32_t kFormatVersion = 1;
  static constexpr uint32_t kHeaderWords = 8;
  static constexpr uint32_t kCellWords = 5;
  static constexpr uint32_t kLookupWidthM = LightCellFrame::kLookupWidthM;
  static constexpr double kPositionWidthM = LightCellFrame::kPositionWidthM;

  std::vector<SubjectLightUniform> Emitters;
  std::vector<uint32_t> Words;
  LightCellCosts Costs;
  bool FullEnumeration = false;
};

[[nodiscard]] std::expected<LightCells, std::string>
BuildLightCells(std::span<const SubjectLight> lights, const LightCellLimits &limits);
}
#endif
