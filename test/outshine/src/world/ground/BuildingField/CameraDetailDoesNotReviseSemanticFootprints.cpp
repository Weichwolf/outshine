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
  OsmField vectors(14, {});
  BuildingField field;
  field.AnchorAt({{0, 0, 0}});
  std::array<BuildingField::Footprint, 1> prints{{{.HeightM = 12.0f}}};
  const std::array<double, 1> spread{0.5}, across{18.0};
  BuildingField::Baked baked{
      .Prints = prints, .SeatSpreadM = spread, .AcrossM = across, .Triangles = 12, .OsmHeights = 1};
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
    return BuildingField::BakeInputs{.HeightRasterDigest = 5,
                                     .StreetDigest = 6,
                                     .FocalPx = focal,
                                     .TileSpanM = 1000,
                                     .Eye = {.LongitudeDeg = eyeLon, .LatitudeDeg = 47}};
  };
  field.PreparesAcceptances({.Prints = 1, .Spread = 1, .Across = 1, .Tiles = 1});
  field.Take(7);
  auto first = field.PrepareAcceptance(7, baked, sources, true, vector, input(720, 9));
  field.CommitAcceptance(std::move(first), vectors, baked);
  const uint64_t semanticRevision = field.Revision();
  baked.Triangles = 3;
  field.PreparesAcceptances({.Prints = 1, .Spread = 1, .Across = 1});
  const std::array repeated{nextHeight, height, height};
  auto camera = field.PrepareAcceptance(7, baked, repeated, true, vector, input(1080, 10));
  field.ReplaceAcceptance(std::move(camera), baked);
  CHECK(field.Revision() == semanticRevision && field.TrianglesHanded() == 3 &&
            field.InputOfTile(7) && field.InputOfTile(7)->Bake.Eye.LongitudeDeg == 10,
        "camera detail and duplicate source delivery do not revise semantic ground");
  CHECK(field.InputOfTile(7) && field.InputOfTile(7)->Sources.size() == 2,
        "accepted source identity stores a canonical set");
  height.Revision = "two";
  sources.front() = height;
  auto source = field.PrepareAcceptance(7, baked, sources, true, vector, input(1080, 10));
  field.ReplaceAcceptance(std::move(source), baked);
  CHECK(field.Revision() == semanticRevision + 1,
        "changed pinned DEM source revises the semantic footprint input");
  prints.front().HeightM = 15.0f;
  auto shape = field.PrepareAcceptance(7, baked, sources, true, vector, input(1080, 10));
  field.ReplaceAcceptance(std::move(shape), baked);
  CHECK(field.Revision() == semanticRevision + 2 && field.OfTile(7).front().HeightM == 15.0f,
        "changed footprint shape revises the semantic ground state");
  return Report();
}
