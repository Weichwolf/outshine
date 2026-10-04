#include "BuildingMesh.h"
#include "StructureBake.h"
#include "Check.h"
#include "Geodesy.h"
#include "math/Units.h"
#include <array>
#include <cmath>
#include <cstdio>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  RawTile raw;
  raw.TileSpanM = 1000;
  const Ground::GeoBounds bounds{
      .MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.02, .MaxLatDeg = 47.02};
  const double metresPerLon = kMPerDegLon * std::cos(47 * kDeg2Rad);
  for (uint32_t row = 0; row < 32; ++row) {
    for (uint32_t column = 0; column < 32; ++column) {
      const double lat = 47 + (row * 20.0) / kMPerDegLat;
      const double lon = 9 + (column * 20.0) / metresPerLon;
      const uint32_t first = static_cast<uint32_t>(raw.LatLon.size() / 2);
      const std::array ring{lat,
                            lon,
                            lat,
                            lon + 12 / metresPerLon,
                            lat + 16 / kMPerDegLat,
                            lon + 12 / metresPerLon,
                            lat + 16 / kMPerDegLat,
                            lon};
      const auto cell = StructureCellOf(bounds, ring);
      CHECK(cell.has_value(), "each source footprint has a cell");
      if (!cell) { return Report(); }
      raw.LatLon.insert(raw.LatLon.end(), ring.begin(), ring.end());
      raw.Structures.push_back(
          {.LocalFirst = first, .PointCount = 4, .Cell = *cell, .HeightM = 9, .Pitched = 0});
    }
  }
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47}, raw.AnchorEcef);
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  BuildingMesh mesher;
  size_t fineTriangles = 0;
  for (const auto detail : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
    raw.RequestedDetail = detail;
    auto scratch = mesher.Scratch();
    BakedTile built;
    CHECK(BakeStructures(raw, *heights, mesher, *scratch, built).has_value(),
          "the complete dense city generates at each requested detail");
    CHECK(built.Prints.size() == 1024 && built.UnsupportedMeshes == 0 && built.NoGround == 0,
          "all 1024 source footprints survive every detail level");
    const size_t triangles = (built.Built.WallRun.size() + built.Built.RoofRun.size()) / 3;
    std::printf("detail %d: %zu triangles, %d aggregated buildings\n",
                static_cast<int>(detail),
                triangles,
                built.Lumped);
    if (detail == LevelOfDetail::Fine) {
      fineTriangles = triangles;
      CHECK(triangles > 1024 * 12,
            "fine housing adds recessed openings to every complete rectangular envelope");
    } else if (detail == LevelOfDetail::Shell) {
      CHECK(triangles == 1024 * 12,
            "the shell keeps eight wall, two floor and two roof triangles per building");
    } else {
      CHECK(built.Lumped == 1024 && triangles > 0 && triangles < fineTriangles / 4,
            "distant clusters retain all footprints with less than a quarter of the triangles");
    }
  }
  return Report();
}
