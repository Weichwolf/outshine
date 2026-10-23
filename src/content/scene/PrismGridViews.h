#ifndef OUTSHINE_CONTENT_SCENE_PRISMGRIDVIEWS_H
#define OUTSHINE_CONTENT_SCENE_PRISMGRIDVIEWS_H

#include "PrismGrid.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine {
struct PrismCellRect {
  uint32_t Patch = 0;
  uint32_t Left = 0, Top = 0, Right = 0, Bottom = 0;
};

struct PrismViewPlacement {
  uint32_t Patch = 0;
  uint32_t Column = 0, Row = 0;
};

struct PrismGridViews {
  PrismGrid Geometry;
  std::vector<PrismViewPlacement> Placements;
  size_t IntersectionTests = 0;
};

struct PrismViewLimits {
  size_t InputRecordsMost = 0;
  size_t ViewsMost = 0;
  size_t IntersectionTestsMost = 0;
};

enum class PrismViewError { InvalidSource, InvalidExclusion, OverBudget };

[[nodiscard]] std::expected<PrismGridViews, PrismViewError> ExcludePrismCells(
    const PrismGrid &source, std::span<const PrismCellRect> exclusions, PrismViewLimits limits);
}
#endif
