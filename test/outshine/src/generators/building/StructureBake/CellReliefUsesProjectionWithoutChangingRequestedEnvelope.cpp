#include "Check.h"
#include "Geodesy.h"
#include "src/generators/building/BuildingMesh.h"
#include "src/generators/building/StructureBake.h"
#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  RawTile raw;
  raw.LatLon = {47, 9, 47, 9.00015, 47.0001, 9.00015, 47.0001, 9};
  const Ground::GeoBounds bounds{
      .MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.008, .MaxLatDeg = 47.008};
  const auto cell = StructureCellOf(bounds, raw.LatLon);
  CHECK(cell.has_value(), "housing receives a valid native cell");
  if (!cell) { return Report(); }
  raw.Structures.push_back({.PointCount = 4, .Cell = *cell, .HeightM = 9});
  raw.RequestedDetail = LevelOfDetail::Fine;
  raw.RequestedCell = cell->Index;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47, .HeightM = 0}, raw.AnchorEcef);
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  BuildingMesh mesher;
  std::array<size_t, 3> triangles{};
  for (size_t mode = 0; mode < triangles.size(); ++mode) {
    raw.Eye = {.LongitudeDeg = mode == 0 ? 9 : 9.02, .LatitudeDeg = 47};
    raw.Projection = {.FocalPx = 1000, .AllowedErrorPx = mode == 2 ? 0.0 : 1.0};
    auto scratch = mesher.Scratch();
    BakedTile baked;
    CHECK(BakeStructures(raw, *heights, mesher, *scratch, baked).has_value(),
          "the requested Fine cell bakes under its view allowance");
    CHECK(baked.FootprintDetails == std::vector{LevelOfDetail::Fine},
          "feature selection preserves the requested envelope LOD");
    triangles[mode] = (baked.Built.WallRun.size() + baked.Built.RoofRun.size()) / 3;
    CHECK(baked.Prints.size() == 1 && baked.Prints.front().HeightM == 9,
          "relief selection preserves the semantic building and its height");
  }
  CHECK(triangles[0] > triangles[1],
        "distant Fine cells retain the shell without recess expansion");
  CHECK(triangles[0] == triangles[2], "zero allowance preserves the full canonical reference");
  return Report();
}
