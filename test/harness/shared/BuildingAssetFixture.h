#ifndef OUTSHINE_TEST_BUILDINGASSETFIXTURE_H
#define OUTSHINE_TEST_BUILDINGASSETFIXTURE_H
#include "StructureBake.h"
#include "StructureCell.h"
#include "HeightField.h"
#include "Geodesy.h"
#include "FacadeProfiles.h"
#include <array>
#include <memory>

namespace outshine::Test::BuildingAssetFixture {
inline Generators::RawTile Inputs() {
  Generators::RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47}, raw.AnchorEcef);
  const Ground::GeoBounds region{
      .MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.02, .MaxLatDeg = 47.02};
  const std::array ring{47.001, 9.001, 47.001, 9.0012, 47.0012, 9.0012, 47.0012, 9.001};
  const auto cell = Generators::StructureCellOf(region, ring);
  if (!cell) { return {}; }
  raw.LatLon.assign(ring.begin(), ring.end());
  raw.Structures.push_back({.LocalFirst = 0,
                            .PointCount = 4,
                            .SourceFirst = 0,
                            .Cell = *cell,
                            .HeightM = 12,
                            .MinimumHeightM = 0,
                            .Pitched = 0,
                            .WallColour = {},
                            .Facade = FacadeStyle::Commercial});
  return raw;
}

inline std::shared_ptr<const Ground::HeightField> Heights() {
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {125, 125, 125, 125};
  return Ground::HeightField::Of(0, {block});
}

}
#endif
