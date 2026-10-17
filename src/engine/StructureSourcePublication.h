#ifndef OUTSHINE_ENGINE_STRUCTURESOURCEPUBLICATION_H
#define OUTSHINE_ENGINE_STRUCTURESOURCEPUBLICATION_H

#include "StructureBuildQueue.h"
#include "TilePieces.h"
#include <array>
#include <bit>
#include <optional>
#include <span>
#include <string>

namespace outshine {
[[nodiscard]] inline bool PublishStructureSource(TilePieces &pieces,
                                                 const StructureBuildQueue::Landing &landing,
                                                 std::string &error) {
  if (landing.InitialCells == 0) {
    return pieces.Hands(landing.Tile, *landing.Baked, landing.AnchorEcef, error, landing.SourceKey);
  }
  const uint32_t first = static_cast<uint32_t>(std::countr_zero(landing.InitialCells)) + 1u;
  if (landing.Baked->RequestedCell == first) { pieces.DiscardStagedCells(landing.Tile); }
  if (!landing.Baked->RequestedCell || !pieces.StageCell(landing.Tile,
                                                         *landing.Baked->RequestedCell,
                                                         *landing.Baked,
                                                         landing.AnchorEcef,
                                                         error,
                                                         landing.SourceKey)) {
    return false;
  }
  if (!landing.SourceComplete) { return true; }
  std::array<TilePieces::CellSelection, Generators::kStructureCellsPerTile> selected{};
  size_t count = 0;
  uint64_t remaining = landing.InitialCells;
  while (remaining != 0) {
    selected[count++] = {.Cell = static_cast<uint32_t>(std::countr_zero(remaining)) + 1u,
                         .Detail = std::nullopt};
    remaining &= remaining - 1u;
  }
  return pieces.ActivateCells(landing.Tile,
                              {.Key = landing.SourceKey, .Occupied = landing.InitialCells},
                              std::span(selected).first(count),
                              error);
}
}
#endif
