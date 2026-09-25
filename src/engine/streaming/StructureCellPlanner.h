#ifndef OUTSHINE_ENGINE_STREAMING_STRUCTURECELLPLANNER_H
#define OUTSHINE_ENGINE_STREAMING_STRUCTURECELLPLANNER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "BuildingField.h"
#include "StructureBuildQueue.h"
#include "StructureCellDetail.h"
#include "TilePieces.h"

namespace outshine {

struct StructureCellPlan {
  std::array<TilePieces::CellSelection, Ground::kStructureCellsPerTile> Selected{};
  size_t Count = 0;
  std::optional<StructureBuildQueue::CellRequest> Missing;
  bool Complete = false;
  bool Active = false;

  [[nodiscard]] std::span<const TilePieces::CellSelection> Choices() const noexcept {
    return {Selected.data(), Count};
  }
};

[[nodiscard]] inline StructureCellPlan
PlanStructureCells(uint32_t tile,
                   const Ground::BuildingField::AcceptedInput &source,
                   uint64_t sourceKey,
                   LongitudeLatitude eye,
                   double focalPx,
                   const TilePieces &pieces,
                   const StructureBuildQueue &queue) noexcept {
  StructureCellPlan plan;
  if (!source.Qualified || sourceKey == 0 || source.OccupiedCells == 0) { return plan; }
  plan.Complete = true;
  for (uint32_t cell = 1; cell <= Ground::kStructureCellsPerTile; ++cell) {
    if ((source.OccupiedCells & (uint64_t{1} << (cell - 1u))) == 0) { continue; }
    const auto detail = RequestedStructureCellDetail(source.CellBounds[cell - 1u],
                                                     source.CellMaxHeightM[cell - 1u],
                                                     source.Bake.TileSpanM,
                                                     eye,
                                                     focalPx);
    plan.Selected[plan.Count++] = {.Cell = cell, .Detail = detail};
    if (pieces.HasCell(tile, cell, detail, sourceKey)) { continue; }
    plan.Complete = false;
    const StructureBuildQueue::CellRequest request{
        .Tile = tile, .Cell = cell, .Detail = detail, .SourceKey = sourceKey};
    if (!plan.Missing && !queue.CellQueued(request)) { plan.Missing = request; }
  }
  plan.Active = plan.Complete && pieces.CellsActive(tile, plan.Choices(), sourceKey);
  return plan;
}

}

#endif
