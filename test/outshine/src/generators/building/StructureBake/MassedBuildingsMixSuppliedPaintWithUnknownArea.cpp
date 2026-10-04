#include "BuildingMaterials.h"
#include "BuildingMesh.h"
#include "StructureBake.h"
#include "Check.h"
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  RawTile raw;
  raw.LatLon = {47.0, 9.0, 47.0, 9.0001, 47.0001, 9.0001, 47.0001, 9.0};
  const Ground::GeoBounds bounds{
      .MinLonDeg = 9.0, .MinLatDeg = 47.0, .MaxLonDeg = 9.008, .MaxLatDeg = 47.008};
  const auto cell = StructureCellOf(bounds, raw.LatLon);
  CHECK(cell.has_value(), "fixture has a valid cell");
  if (!cell) { return Report(); }
  const Vec3f paint{{0.2f, 0.4f, 0.6f}};
  raw.Structures.push_back({.PointCount = 4, .Cell = *cell, .HeightM = 12.0});
  raw.Structures.push_back({.PointCount = 4, .Cell = *cell, .HeightM = 12.0, .WallColour = paint});
  raw.TileSpanM = 1000.0;
  raw.RequestedDetail = LevelOfDetail::Massed;
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  BakedTile built;
  CHECK(BakeStructures(raw, *heights, mesher, *scratch, built).has_value(),
        "equal-area buildings generate one far representation");
  CHECK(built.Lumped == 2 && !built.Built.WallColours.empty() &&
            built.Built.WallColours.size() == built.Built.WallCorners.size() * 4,
        "far representation retains mixed source paint");
  if (built.Built.WallColours.size() != built.Built.WallCorners.size() * 4) { return Report(); }
  size_t painted = 0;
  for (size_t at = 0; at < built.Built.WallCorners.size(); ++at) {
    const float u = built.Built.WallCorners[at].uv()[0];
    if (u < 0.0f && std::fmod(-u - 1.0f, 16.0f) != 0.0f) { continue; }
    ++painted;
    for (size_t channel = 0; channel < 3; ++channel) {
      CHECK_NEAR(built.Built.WallColours[at * 4 + channel] * kBuildingWallColour[channel],
                 (paint[channel] + kBuildingWallColour[channel]) * 0.5f,
                 0.000001,
                 "linear area mixture",
                 "unknown area contributes the default paint");
    }
  }
  CHECK(painted > 0, "the far product contains observable opaque walls");
  return Report();
}
