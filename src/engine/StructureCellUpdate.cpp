#include "EngineState.h"
#include "StructureCellPlanner.h"
#include "StructureTilePublication.h"
#include <span>
#include <utility>

namespace outshine {
bool Engine::State::AdvanceStructureCells(LongitudeLatitude eye,
                                          const StructureBuildQueue::HeightSource &heightAt) {
  auto &footprints = World.Stack.Footprints();
  const auto ready = World.StructureBuilds.NextCellLanding(World.Stack, footprints, heightAt);
  if (!ready) {
    Error = Generators::Describe(ready.error());
    return false;
  }
  if (*ready) {
    const auto staged = StageStructureCell(World, Picture.Device, **ready);
    if (!staged) {
      Error = staged.error();
      return false;
    }
    World.StructureBuilds.CommitsCellLanding(**ready);
  }
  for (const auto tile : footprints.AcceptedTiles()) {
    const auto *input = footprints.InputOfTile(tile);
    if (input == nullptr || !input->Bake.PreparedAssetKey || input->OccupiedCells == 0) {
      continue;
    }
    const auto key = StructureBuildQueue::QualifiedSourceKey(footprints, tile);
    if (!key) { continue; }
    const auto plan = PlanStructureCells(
        tile, *input, *key, eye, footprints.Projection(), World.Pieces, World.StructureBuilds);
    if (plan.Complete && !plan.Active) {
      const auto activated = ActivateStructureCells(
          World, Picture.Device, tile, *key, input->OccupiedCells, plan.Choices());
      if (!activated) {
        Error = activated.error();
        return false;
      }
    }
    if (plan.MissingCount == 0) { continue; }
    const auto posted =
        World.StructureBuilds.PostsCells(World.Stack,
                                         footprints,
                                         eye,
                                         heightAt,
                                         std::span(plan.MissingBatch).first(plan.MissingCount));
    if (!posted) {
      Error = posted.error();
      return false;
    }
  }
  return true;
}
}
