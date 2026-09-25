#include "StructureCellPlanner.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Ground::BuildingField::AcceptedInput source;
  source.Qualified = true;
  source.OccupiedCells = 3;
  source.Bake.TileSpanM = 1000;
  source.CellBounds[0] = {
      .MinLonDeg = 9.0, .MinLatDeg = 47.0, .MaxLonDeg = 9.001, .MaxLatDeg = 47.001};
  source.CellBounds[1] = {
      .MinLonDeg = 9.002, .MinLatDeg = 47.0, .MaxLonDeg = 9.003, .MaxLatDeg = 47.001};
  source.CellMaxHeightM[0] = 12;
  source.CellMaxHeightM[1] = 16;
  TilePieces pieces;
  StructureBuildQueue queue;
  const auto near = PlanStructureCells(
      7, source, 42, {.LongitudeDeg = 9.0005, .LatitudeDeg = 47.0005}, 720, pieces, queue);
  CHECK(near.Count == 2 && near.Choices()[0].Cell == 1 && near.Choices()[1].Cell == 2 &&
            near.Choices()[0].Detail == LevelOfDetail::Fine && near.Missing &&
            near.Missing->Tile == 7 && near.Missing->Cell == 1 && !near.Complete && !near.Active,
        "nearby source selects ordered cell variants and one bounded missing request");
  const auto far = PlanStructureCells(
      7, source, 42, {.LongitudeDeg = 11.0, .LatitudeDeg = 47.0}, 720, pieces, queue);
  CHECK(far.Count == 2 && far.Choices()[0].Detail == LevelOfDetail::Shell && far.Missing &&
            far.Missing->Detail == LevelOfDetail::Shell,
        "view distance changes requested detail without changing source identity");
  source.Qualified = false;
  const auto unqualified = PlanStructureCells(
      7, source, 42, {.LongitudeDeg = 9.0, .LatitudeDeg = 47.0}, 720, pieces, queue);
  CHECK(unqualified.Count == 0 && !unqualified.Missing && !unqualified.Complete,
        "unqualified semantic input cannot produce detail requests");
  return Report();
}
