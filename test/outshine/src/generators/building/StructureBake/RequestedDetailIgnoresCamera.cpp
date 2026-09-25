#include "src/generators/building/BuildingMesh.h"
#include "src/generators/building/StructureBake.h"
#include "Check.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  Generators::RawTile raw;
  raw.LatLon = {47.0, 9.0, 47.0, 9.0001, 47.0001, 9.0001, 47.0001, 9.0};
  const Ground::GeoBounds tileBounds{
      .MinLonDeg = 9.0, .MinLatDeg = 47.0, .MaxLonDeg = 9.008, .MaxLatDeg = 47.008};
  const auto fixtureCell = Generators::StructureCellOf(tileBounds, raw.LatLon);
  CHECK(fixtureCell.has_value(), "valid synthetic building receives a cell");
  if (!fixtureCell) { return Report(); }
  raw.Structures.push_back({.PointCount = 4, .Cell = *fixtureCell, .HeightM = 12.0});
  raw.TileSpanM = 1000.0;
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  Generators::BuildingMesh mesher;

  std::array<uint64_t, 3> digests{};
  std::optional<Ground::BuildingField::Footprint> semantic;
  for (const LevelOfDetail detail :
       {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
    raw.RequestedDetail = detail;
    raw.Eye = {.LongitudeDeg = 9.0, .LatitudeDeg = 47.0};
    raw.FocalPx = 1000.0;
    auto nearScratch = mesher.Scratch();
    Generators::BakedTile near;
    CHECK(Generators::BakeStructures(raw, *heights, mesher, *nearScratch, near).has_value(),
          "requested structure detail bakes near the source");

    raw.Eye = {.LongitudeDeg = 0.0, .LatitudeDeg = 0.0};
    raw.FocalPx = 1.0;
    auto farScratch = mesher.Scratch();
    Generators::BakedTile far;
    CHECK(Generators::BakeStructures(raw, *heights, mesher, *farScratch, far).has_value(),
          "the same requested detail bakes for another camera");
    CHECK(near.RequestedDetail == detail && far.RequestedDetail == detail &&
              near.Prints.size() == 1 && far.Prints.size() == 1 &&
              near.OccupiedCells == (uint64_t{1} << (fixtureCell->Index - 1u)) &&
              far.OccupiedCells == near.OccupiedCells &&
              near.CellBounds[fixtureCell->Index - 1u] == fixtureCell->Footprint &&
              far.CellBounds == near.CellBounds &&
              near.CellMaxHeightM[fixtureCell->Index - 1u] == near.Prints.front().HeightM &&
              far.CellMaxHeightM == near.CellMaxHeightM &&
              near.FootprintDetails == std::vector{detail} &&
              far.FootprintDetails == std::vector{detail} && near.Digest == far.Digest,
          "explicit detail geometry and product identity ignore eye and focal length");
    if (semantic) {
      const auto &print = near.Prints.front();
      CHECK(print.FirstPoint == semantic->FirstPoint && print.PointCount == semantic->PointCount &&
                print.HeightM == semantic->HeightM && print.BaseM == semantic->BaseM &&
                print.SeatM == semantic->SeatM && print.FootM == semantic->FootM &&
                print.Source == semantic->Source && print.Street.Known == semantic->Street.Known,
            "the semantic footprint is identical across all render detail levels");
    } else {
      semantic = near.Prints.front();
    }
    digests[static_cast<size_t>(detail)] = near.Digest;
  }
  CHECK(digests[0] != digests[1], "Fine and Shell are distinct source-keyed products");

  raw.RequestedDetail = LevelOfDetail::Fine;
  raw.Structures.front().HeightM = 18.0;
  auto changedScratch = mesher.Scratch();
  Generators::BakedTile changed;
  CHECK(Generators::BakeStructures(raw, *heights, mesher, *changedScratch, changed).has_value() &&
            changed.Digest != digests[0],
        "source geometry changes the requested product digest");

  raw.RequestedDetail = LevelOfDetail::Skyline;
  auto invalidScratch = mesher.Scratch();
  Generators::BakedTile invalid;
  const auto rejected = Generators::BakeStructures(raw, *heights, mesher, *invalidScratch, invalid);
  CHECK(!rejected &&
            std::get_if<Generators::StructureBakeErrorKind>(&rejected.error()) != nullptr &&
            *std::get_if<Generators::StructureBakeErrorKind>(&rejected.error()) ==
                Generators::StructureBakeErrorKind::InvalidDetail,
        "unsupported detail requests fail before any product publication");

  raw.RequestedDetail = LevelOfDetail::Fine;
  raw.Structures.push_back(raw.Structures.front());
  Generators::StructureBakeProgress progress;
  auto firstScratch = mesher.Scratch();
  const auto first = progress.AdvanceStructures(raw, *heights, mesher, *firstScratch, 1);
  CHECK(first && !*first && progress.BakedStructures() == 1,
        "the first slice pins its requested detail");
  raw.RequestedDetail = LevelOfDetail::Shell;
  const auto mixed = progress.AdvanceStructures(raw, *heights, mesher, *firstScratch, 1);
  CHECK(!mixed && std::get_if<Generators::StructureBakeErrorKind>(&mixed.error()) != nullptr &&
            *std::get_if<Generators::StructureBakeErrorKind>(&mixed.error()) ==
                Generators::StructureBakeErrorKind::ChangedDetail,
        "one tile cannot mix detail requests across worker slices");

  const std::array westRing{47.004, 9.0005, 47.004, 9.0015, 47.0041, 9.0015, 47.0041, 9.0005};
  const std::array eastRing{47.004, 9.0065, 47.004, 9.0075, 47.0041, 9.0075, 47.0041, 9.0065};
  const auto westCell = Generators::StructureCellOf(tileBounds, westRing);
  const auto eastCell = Generators::StructureCellOf(tileBounds, eastRing);
  const std::array reversedWest{47.0041, 9.0005, 47.0041, 9.0015, 47.004, 9.0015, 47.004, 9.0005};
  const auto reversedCell = Generators::StructureCellOf(tileBounds, reversedWest);
  CHECK(westCell && eastCell && reversedCell && westCell->Index != eastCell->Index &&
            westCell->Index == reversedCell->Index && westCell->Footprint.MinLonDeg < 9.001 &&
            westCell->Footprint.MaxLonDeg > 9.001,
        "tile-local cell ownership survives ring order and keeps the entire cross-cell footprint");
  const Ground::GeoBounds datelineTile{
      .MinLonDeg = 179.9, .MinLatDeg = 0, .MaxLonDeg = 180.0, .MaxLatDeg = 0.1};
  const std::array datelineRing{0.05, 179.99, 0.05, -179.99, 0.06, -179.99, 0.06, 179.99};
  const auto datelineCell = Generators::StructureCellOf(datelineTile, datelineRing);
  CHECK(datelineCell && datelineCell->Footprint.MinLonDeg == 179.99 &&
            datelineCell->Footprint.MaxLonDeg > 180.0,
        "antimeridian crossing stays local and does not acquire world-size bounds");
  auto invalidRing = westRing;
  invalidRing[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!Generators::StructureCellOf(tileBounds, invalidRing),
        "nonfinite source positions cannot obtain a cell identity");
  if (!westCell || !eastCell) { return Report(); }

  Generators::RawTile cellRaw;
  cellRaw.LatLon.assign(westRing.begin(), westRing.end());
  cellRaw.LatLon.insert(cellRaw.LatLon.end(), eastRing.begin(), eastRing.end());
  cellRaw.Structures = {
      {.PointCount = 4, .SourceFirst = 0, .Cell = *westCell, .HeightM = 12.0},
      {.LocalFirst = 4, .PointCount = 4, .SourceFirst = 4, .Cell = *eastCell, .HeightM = 16.0}};
  cellRaw.TileSpanM = 1000.0;
  cellRaw.FocalPx = 1000.0;
  cellRaw.RequestedDetail = LevelOfDetail::Fine;
  cellRaw.RequestedCell = westCell->Index;
  auto westScratch = mesher.Scratch();
  Generators::BakedTile west;
  CHECK(Generators::BakeStructures(cellRaw, *heights, mesher, *westScratch, west).has_value() &&
            west.RequestedCell == westCell->Index && west.Prints.size() == 1 &&
            west.OccupiedCells == (uint64_t{1} << (westCell->Index - 1u)) &&
            west.CellBounds[westCell->Index - 1u] == westCell->Footprint &&
            west.CellMaxHeightM[westCell->Index - 1u] == west.Prints.front().HeightM &&
            west.Prints.front().FirstPoint == 0 && west.FootprintBounds &&
            west.FootprintBounds->MinLonDeg == westCell->Footprint.MinLonDeg &&
            west.FootprintBounds->MaxLonDeg == westCell->Footprint.MaxLonDeg,
        "one cell bake owns only its structure and the full footprint bounds");
  cellRaw.Eye = {.LongitudeDeg = 0, .LatitudeDeg = 0};
  cellRaw.FocalPx = 1;
  auto movedScratch = mesher.Scratch();
  Generators::BakedTile moved;
  CHECK(Generators::BakeStructures(cellRaw, *heights, mesher, *movedScratch, moved).has_value() &&
            moved.Digest == west.Digest,
        "cell product is independent of the camera and focal length");
  cellRaw.RequestedCell = eastCell->Index;
  auto eastScratch = mesher.Scratch();
  Generators::BakedTile east;
  CHECK(Generators::BakeStructures(cellRaw, *heights, mesher, *eastScratch, east).has_value() &&
            east.Prints.size() == 1 && east.Prints.front().FirstPoint == 4 &&
            east.OccupiedCells == (uint64_t{1} << (eastCell->Index - 1u)) &&
            east.CellBounds[eastCell->Index - 1u] == eastCell->Footprint &&
            east.CellMaxHeightM[eastCell->Index - 1u] == east.Prints.front().HeightM &&
            east.Digest != west.Digest,
        "a neighbouring cell produces a distinct product from the same source tile");
  const std::array widerWestRing{
      47.0041, 9.0011, 47.0041, 9.0018, 47.0042, 9.0018, 47.0042, 9.0011};
  const auto extraCell = Generators::StructureCellOf(tileBounds, widerWestRing);
  CHECK(extraCell && extraCell->Index == westCell->Index,
        "the second footprint belongs to the same cell");
  if (!extraCell) { return Report(); }
  auto unionRaw = cellRaw;
  unionRaw.RequestedCell = westCell->Index;
  unionRaw.LatLon.insert(unionRaw.LatLon.end(), widerWestRing.begin(), widerWestRing.end());
  unionRaw.Structures.push_back(
      {.LocalFirst = 8, .PointCount = 4, .SourceFirst = 8, .Cell = *extraCell, .HeightM = 24.0});
  auto unionScratch = mesher.Scratch();
  Generators::BakedTile united;
  CHECK(Generators::BakeStructures(unionRaw, *heights, mesher, *unionScratch, united).has_value() &&
            united.Prints.size() == 2 &&
            united.CellBounds[westCell->Index - 1u].MinLonDeg == westCell->Footprint.MinLonDeg &&
            united.CellBounds[westCell->Index - 1u].MaxLonDeg == extraCell->Footprint.MaxLonDeg &&
            united.CellMaxHeightM[westCell->Index - 1u] == 24.0f,
        "one cell envelope includes both full footprints and their greatest height");
  cellRaw.RequestedCell = 0;
  auto invalidCellScratch = mesher.Scratch();
  Generators::BakedTile invalidCell;
  const auto badCell =
      Generators::BakeStructures(cellRaw, *heights, mesher, *invalidCellScratch, invalidCell);
  CHECK(!badCell && std::get_if<Generators::StructureBakeErrorKind>(&badCell.error()) != nullptr &&
            *std::get_if<Generators::StructureBakeErrorKind>(&badCell.error()) ==
                Generators::StructureBakeErrorKind::InvalidCell,
        "reserved whole-tile address cannot be requested as a cell product");
  cellRaw.RequestedCell = eastCell->Index;
  cellRaw.Structures.front().Cell.Index = 0;
  auto invalidSourceScratch = mesher.Scratch();
  Generators::BakedTile invalidSource;
  const auto badSource =
      Generators::BakeStructures(cellRaw, *heights, mesher, *invalidSourceScratch, invalidSource);
  CHECK(!badSource &&
            std::get_if<Generators::StructureBakeErrorKind>(&badSource.error()) != nullptr &&
            *std::get_if<Generators::StructureBakeErrorKind>(&badSource.error()) ==
                Generators::StructureBakeErrorKind::InvalidCell,
        "a malformed source in another cell cannot be silently omitted");
  cellRaw.Structures.front().Cell = *westCell;
  cellRaw.RequestedCell = westCell->Index;
  Generators::StructureBakeProgress cellProgress;
  auto slicedScratch = mesher.Scratch();
  const auto firstCell =
      cellProgress.AdvanceStructures(cellRaw, *heights, mesher, *slicedScratch, 1);
  CHECK(firstCell && !*firstCell, "first worker slice pins the selected cell");
  cellRaw.RequestedCell = eastCell->Index;
  const auto changedCell =
      cellProgress.AdvanceStructures(cellRaw, *heights, mesher, *slicedScratch, 1);
  CHECK(!changedCell &&
            std::get_if<Generators::StructureBakeErrorKind>(&changedCell.error()) != nullptr &&
            *std::get_if<Generators::StructureBakeErrorKind>(&changedCell.error()) ==
                Generators::StructureBakeErrorKind::ChangedCell,
        "worker slices reject a changed cell request");
  return Report();
}
