#include "PreparedStructureTile.h"
#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "HeightField.h"
#include "Check.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <variant>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 100}, raw.AnchorEcef);
  raw.LatLon = {48.001, 16.001, 48.001, 16.0012, 48.0012, 16.0012, 48.0012, 16.001};
  raw.Structures.push_back({.LocalFirst = 0,
                            .PointCount = 4,
                            .SourceFirst = 50,
                            .SourceFirstHole = 3,
                            .Cell = {.Index = 1, .Footprint = {}},
                            .HeightM = 30,
                            .Pitched = 0,
                            .WallColour = {},
                            .Facade = {}});
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100, 100, 100, 100};
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights, "the fixture has deterministic elevation inputs");
  if (!heights) { return Report(); }
  auto base = PrepareStructureTile(raw, *heights);
  CHECK(base, "source preparation stores complete terrain contact products");
  if (!base) { return Report(); }
  BuildingScratch scratch;
  const BuildingMesh mesher;
  raw.Projection.AllowedErrorPx = 0;
  const auto reference = BakePreparedStructures(*base, raw, mesher, scratch);
  CHECK(reference && !reference->Built.WallRun.empty(),
        "the prior render path emits real geometry");
  if (!reference) { return Report(); }
  base->Surfaces.clear();
  const auto contacts = StructureTerrainContacts(*base);
  CHECK(contacts && contacts->Prints == reference->Prints &&
            contacts->SeatSpreadM == reference->SeatSpreadM &&
            contacts->AcrossM == reference->AcrossM &&
            contacts->OccupiedCells == reference->OccupiedCells &&
            contacts->CellBounds == reference->CellBounds &&
            contacts->CellMaxHeightM == reference->CellMaxHeightM,
        "terrain metadata exactly matches the render path without any prepared mesh surfaces");
  if (!contacts) { return Report(); }
  CHECK(contacts->Built.WallCorners.empty() && contacts->Built.RoofCorners.empty() &&
            contacts->Built.WallRun.empty() && contacts->Built.RoofRun.empty() &&
            !contacts->SelectedView && contacts->GeometryAssetKey.empty() &&
            std::ranges::all_of(contacts->CellShellErrorM, [](double error) { return error < 0; }),
        "contact readiness publishes neither render geometry nor a LOD or view-domain claim");
  CHECK(std::ranges::equal(contacts->Coordinates->Points(), reference->Coordinates->Points()) &&
            contacts->Coordinates->Rings().size() == reference->Coordinates->Rings().size(),
        "the full coordinate owner survives independently of render geometry");
  const std::atomic_bool cancelled{true};
  const auto stopped = StructureTerrainContacts(*base, &cancelled);
  CHECK(!stopped && stopped.error() == StructureBakeError{StructureBakeErrorKind::Cancelled},
        "cancellation never publishes partial contact coverage");
  base->Structures[0].Layout.Cell.Index = 0;
  CHECK(!StructureTerrainContacts(*base),
        "invalid source ownership fails before contact publication");
  return Report();
}
