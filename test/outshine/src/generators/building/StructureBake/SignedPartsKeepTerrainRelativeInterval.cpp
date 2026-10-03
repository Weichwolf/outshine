#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "BuildingShape.h"
#include "Check.h"
#include "Geodesy.h"
#include "Sha256.h"
#include "StructureArtifact.h"
#include "StructureBake.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  RawTile raw;
  raw.LatLon = {47, 9, 47, 9.0002, 47.0002, 9.0002, 47.0002, 9};
  const auto cell = StructureCellOf(
      {.MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.008, .MaxLatDeg = 47.008}, raw.LatLon);
  CHECK(cell.has_value(), "signed footprint belongs to a spatial cell");
  if (!cell) { return Report(); }
  raw.Structures.push_back({.PointCount = 4,
                            .Cell = *cell,
                            .Pitched = 0,
                            .HeightOrigin = Ground::BuildingHeightOrigin::Generated});
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
  BuildingScratch shapeScratch;
  const auto key = Sha256Hex("signed-height-interval");
  for (const auto interval :
       {std::array{4.0, -11.0}, std::array{-2.0, -11.0}, std::array{0.0, -1.0}}) {
    raw.Structures[0].HeightM = interval[0];
    raw.Structures[0].MinimumHeightM = interval[1];
    const auto shape = MassOf(
        raw.LatLon, {.HeightM = interval[0], .MinimumHeightM = interval[1]}, {}, shapeScratch);
    CHECK(shape && shape->size() == 1 && shape->front().FootM == interval[1] &&
              std::abs(shape->front().TopM() - interval[0]) < 1e-9,
          "signed source interval remains one exact body without storey inflation");
    for (const auto level : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
      raw.RequestedDetail = level;
      BakedTile product;
      const auto baked = BakeStructures(raw, *heights, mesher, *scratch, product);
      CHECK(baked.has_value(), "below-grade and crossing parts bake at every enabled detail");
      if (!baked) { continue; }
      CHECK(product.Prints.size() == 1 && product.Prints[0].HeightM == interval[0] &&
                product.Prints[0].MinimumHeightM == interval[1] && product.Lumped == 0 &&
                product.DefaultHeights == 1 && product.OsmHeights == 0,
            "native product preserves signed endpoints and estimated provenance");
      double lower = std::numeric_limits<double>::infinity();
      double upper = -lower;
      for (const auto &vertices :
           {std::span(product.Built.WallCorners), std::span(product.Built.RoofCorners)}) {
        for (const auto &vertex : vertices) {
          double up = 0;
          for (int axis = 0; axis < 3; ++axis) { up += vertex.pos[axis] * axes.Up[axis]; }
          lower = std::min(lower, up);
          upper = std::max(upper, up);
        }
      }
      CHECK(std::isfinite(lower) && std::abs(lower - interval[1]) < 0.02,
            "independent ENU projection verifies the original below-grade lower surface");
      if (level != LevelOfDetail::Fine) {
        CHECK(std::isfinite(upper) && std::abs(upper - interval[0]) < 0.02,
              "shell and massed upper surface remains at the source endpoint");
      }
      const auto encoded = EncodeStructureArtifact(product, key);
      const auto decoded = encoded ? DecodeStructureArtifact(*encoded, key, 1) : std::nullopt;
      CHECK(decoded && decoded->Prints == product.Prints,
            "native product transport preserves negative endpoints without a disk cache");
    }
  }
  const auto overflow =
      MassOf(raw.LatLon, {.HeightM = 4, .MinimumHeightM = -1e100}, {}, shapeScratch);
  CHECK(!overflow, "finite lower endpoint cannot overflow storey or float geometry ranges");
  return Report();
}
