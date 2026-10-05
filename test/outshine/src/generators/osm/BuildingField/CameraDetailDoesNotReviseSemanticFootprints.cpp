#include "BuildingField.h"
#include "Check.h"

#include <array>
#include <cstdint>
#include <span>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  ::outshine::Generators::Osm::OsmField vectors(14, {});
  ::outshine::Generators::Osm::BuildingField field;
  field.AnchorAt({{0, 0, 0}});
  std::array<::outshine::Ground::BuildingFootprint, 1> prints{{{.HeightM = 12.0f}}};
  const std::array<double, 1> spread{0.5}, across{18.0};
  ::outshine::Generators::Osm::BuildingField::Baked baked{.Prints = prints,
                                                          .SeatSpreadM = spread,
                                                          .AcrossM = across,
                                                          .OccupiedCells = 1,
                                                          .Triangles = 12,
                                                          .OsmHeights = 1};
  baked.CellBounds[0] = {.MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.01, .MaxLatDeg = 47.01};
  baked.CellMaxHeightM[0] = 12.0f;
  baked.CellShellErrorM[0] = 0.25;
  const Data::TileSourceIdentity vector{.Kind = Data::DataKind::VectorMap,
                                        .Tile = {.Zoom = 14, .X = 7, .Y = 8},
                                        .SourceId = "osm",
                                        .Revision = "one"};
  Data::TileSourceIdentity height{.Kind = Data::DataKind::Elevation,
                                  .Tile = {.Zoom = 15, .X = 14, .Y = 16},
                                  .SourceId = "dem",
                                  .Revision = "one"};
  auto nextHeight = height;
  nextHeight.Tile.X += 1;
  std::array sources{height, nextHeight};
  const auto input = [](double focal, double eyeLon) {
    return ::outshine::Generators::Osm::BuildingField::BakeInputs{
        .HeightRasterDigest = 5,
        .StreetDigest = 6,
        .Projection = {.FocalPx = focal},
        .TileSpanM = 1000,
        .Eye = {.LongitudeDeg = eyeLon, .LatitudeDeg = 47}};
  };
  field.PreparesAcceptances({.Prints = 1, .Spread = 1, .Across = 1, .Tiles = 1});
  field.Take(7);
  auto first = field.PrepareAcceptance(7, baked, sources, true, vector, input(720, 9));
  field.CommitAcceptance(std::move(first), vectors, baked);
  CHECK(field.AcceptedTiles().size() == 1 && field.AcceptedTiles().front() == 7 &&
            field.InputOfTile(field.AcceptedTiles().front()) != nullptr,
        "accepted tile IDs address the same source inputs used by detail planning");
  const uint64_t semanticRevision = field.Revision();
  baked.Triangles = 3;
  field.PreparesAcceptances({.Prints = 1, .Spread = 1, .Across = 1});
  const std::array repeated{nextHeight, height, height};
  auto camera = field.PrepareAcceptance(7, baked, repeated, true, vector, input(1080, 10));
  field.ReplaceAcceptance(std::move(camera), baked);
  CHECK(field.Revision() == semanticRevision && field.TrianglesHanded() == 3 &&
            field.InputOfTile(7) && field.InputOfTile(7)->Bake.Eye.LongitudeDeg == 10 &&
            field.InputOfTile(7)->OccupiedCells == 1,
        "camera detail and duplicate source delivery do not revise semantic ground");
  CHECK(field.InputOfTile(7) && field.InputOfTile(7)->CellBounds == baked.CellBounds &&
            field.InputOfTile(7)->CellMaxHeightM == baked.CellMaxHeightM &&
            field.InputOfTile(7)->CellShellErrorM == baked.CellShellErrorM,
        "accepted cell envelopes are stable source data across camera changes");
  CHECK(field.InputOfTile(7) && field.InputOfTile(7)->Sources.size() == 2,
        "accepted source identity stores a canonical set");
  baked.CellShellErrorM[0] = 0.5;
  auto rounding = field.PrepareAcceptance(7, baked, sources, true, vector, input(1080, 10));
  field.ReplaceAcceptance(std::move(rounding), baked);
  CHECK(field.Revision() == semanticRevision && field.InputOfTile(7)->CellShellErrorM[0] == 0.5,
        "a changed render bound updates selection without rebuilding semantic terrain");
  height.Revision = "two";
  sources.front() = height;
  auto source = field.PrepareAcceptance(7, baked, sources, true, vector, input(1080, 10));
  field.ReplaceAcceptance(std::move(source), baked);
  CHECK(field.Revision() == semanticRevision + 1,
        "changed pinned DEM source revises the semantic footprint input");
  baked.OccupiedCells = 2;
  baked.CellBounds[1] = baked.CellBounds[0];
  baked.CellBounds[0] = {};
  baked.CellMaxHeightM[1] = baked.CellMaxHeightM[0];
  baked.CellMaxHeightM[0] = 0;
  auto cells = field.PrepareAcceptance(7, baked, sources, true, vector, input(1080, 10));
  field.ReplaceAcceptance(std::move(cells), baked);
  CHECK(field.Revision() == semanticRevision + 2 && field.InputOfTile(7) &&
            field.InputOfTile(7)->OccupiedCells == 2,
        "changed spatial cell occupancy revises the accepted source geometry");
  prints.front().HeightM = 15.0f;
  baked.CellMaxHeightM[1] = 15.0f;
  auto shape = field.PrepareAcceptance(7, baked, sources, true, vector, input(1080, 10));
  field.ReplaceAcceptance(std::move(shape), baked);
  CHECK(field.Revision() == semanticRevision + 3 && field.OfTile(7).front().HeightM == 15.0f,
        "changed footprint shape revises the semantic ground state");
  return Report();
}
