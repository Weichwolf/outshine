#include "StructureBake.h"
#include "StructureArtifact.h"
#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "BuildingShape.h"
#include "Geodesy.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  RawTile raw;
  raw.LatLon = {47, 9, 47, 9.0002, 47.0002, 9.0002, 47.0002, 9};
  const Ground::GeoBounds bounds{
      .MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.008, .MaxLatDeg = 47.008};
  const auto cell = StructureCellOf(bounds, raw.LatLon);
  CHECK(cell.has_value(), "raised building belongs to a spatial cell");
  if (!cell) { return Report(); }
  raw.Structures.push_back(
      {.PointCount = 4, .Cell = *cell, .HeightM = 12, .MinimumHeightM = 5, .Pitched = 0});
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47, .HeightM = 100}, raw.AnchorEcef);
  const auto axes = EnuAxesEcef({.LongitudeDeg = 9, .LatitudeDeg = 47});
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 100);
  const auto heights = Ground::HeightField::Of(0, {block});
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  for (const auto level : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
    raw.RequestedDetail = level;
    BakedTile product;
    const auto baked = BakeStructures(raw, *heights, mesher, *scratch, product);
    CHECK(baked.has_value(), "raised building bakes at each enabled detail");
    if (!baked) { continue; }
    CHECK(product.Prints.size() == 1 && product.Prints[0].MinimumHeightM == 5 &&
              product.Lumped == 0,
          "raised source retains its clearance and is not mixed into grounded aggregates");
    double lower = std::numeric_limits<double>::infinity();
    for (const auto &vertices :
         {std::span(product.Built.WallCorners), std::span(product.Built.RoofCorners)}) {
      for (const auto &vertex : vertices) {
        double up = 0;
        for (int axis = 0; axis < 3; ++axis) { up += vertex.pos[axis] * axes.Up[axis]; }
        lower = std::min(lower, up);
      }
    }
    CHECK(std::isfinite(lower) && std::abs(lower - 5) < 0.02,
          "independent ENU projection places the lowest generated surface at min_height");
  }
  BuildingScratch massScratch;
  const auto thin = MassOf(
      raw.LatLon, {.HeightM = 5.5, .MinimumHeightM = 5, .HeightMeasured = true}, {}, massScratch);
  CHECK(thin && thin->size() == 1 && std::abs(thin->front().TopM() - 5.5) < 1e-9 &&
            thin->front().FootM == 5,
        "thin raised parts are not inflated to a default storey height");
  const auto key = StructureArtifactKey(raw, *heights, std::nullopt, mesher.ArtifactVersion());
  raw.Structures[0].MinimumHeightM = 6;
  CHECK(key && key != StructureArtifactKey(raw, *heights, std::nullopt, mesher.ArtifactVersion()),
        "input identity includes the lower elevation");
  for (const double invalid : {-1.0, 12.0, 13.0, std::numeric_limits<double>::quiet_NaN()}) {
    raw.Structures[0].MinimumHeightM = invalid;
    BakedTile product;
    CHECK(!BakeStructures(raw, *heights, mesher, *scratch, product),
          "negative nonfinite and inverted intervals cannot generate geometry");
  }
  return Report();
}
