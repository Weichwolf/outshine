#include "OriginalBuildingInput.h"
#include "StructureBake.h"
#include "StructureArtifact.h"
#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "BuildingShape.h"
#include "Geodesy.h"
#include "Check.h"
#include "OsmXmlReader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <string>

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
  raw.RequestedDetail = LevelOfDetail::Fine;
  raw.Structures[0].HeightM = 5;
  raw.Structures[0].MinimumHeightM = 0;
  std::optional<std::string> previousKey;
  for (const auto origin : {outshine::Ground::BuildingHeightOrigin::Declared,
                            outshine::Ground::BuildingHeightOrigin::Storeys,
                            outshine::Ground::BuildingHeightOrigin::Generated}) {
    raw.Structures[0].HeightOrigin = origin;
    BakedTile product;
    const auto baked = BakeStructures(raw, *heights, mesher, *scratch, product);
    const bool measured = origin == outshine::Ground::BuildingHeightOrigin::Declared;
    CHECK(baked && product.Prints.size() == 1 && product.Prints[0].HeightM == 5 &&
              (product.Prints[0].Source == ::outshine::Ground::BuildingHeightSource::Declared) ==
                  measured &&
              product.OsmHeights == (measured ? 1 : 0) &&
              product.DefaultHeights == (measured ? 0 : 1),
          "resolved five-metre heights bypass tile-default inference without losing provenance");
    const auto identity =
        StructureArtifactKey(raw, *heights, std::nullopt, mesher.ArtifactVersion());
    CHECK(identity && identity != previousKey,
          "height provenance participates in product identity");
    previousKey = identity;
  }
  raw.Structures[0].HeightM = 12;
  raw.Structures[0].MinimumHeightM = 5;
  raw.Structures[0].HeightOrigin.reset();
  const auto thin = MassOf(
      raw.LatLon, {.HeightM = 5.5, .MinimumHeightM = 5, .HeightMeasured = true}, {}, massScratch);
  CHECK(thin && thin->size() == 1 && std::abs(thin->front().TopM() - 5.5) < 1e-9 &&
            thin->front().FootM == 5,
        "thin raised parts are not inflated to a default storey height");
  const auto key = StructureArtifactKey(raw, *heights, std::nullopt, mesher.ArtifactVersion());
  raw.Structures[0].MinimumHeightM = 6;
  CHECK(key && key != StructureArtifactKey(raw, *heights, std::nullopt, mesher.ArtifactVersion()),
        "input identity includes the lower elevation");
  auto original = outshine::Generators::Osm::XmlReader::Read(
      R"(<osm version="0.6"><node id="7" lat="47" lon="9"><tag k="building" v="yes"/></node></osm>)",
      {.DatasetId = "native", .Revision = "one"});
  CHECK(original.has_value(), "native source fixture parses");
  if (!original) { return Report(); }
  raw.SourceInputs.Objects = std::make_shared<const outshine::Generators::Osm::SourceCapture>(
      std::make_shared<const outshine::Generators::Osm::SourceSnapshot>(
          outshine::Generators::Osm::SourceSnapshot{.Elements = std::move(*original),
                                                    .Coverage = {}}));
  raw.Structures[0].SourceId = {
      .Id = 7, .Kind = static_cast<uint8_t>(outshine::Generators::Osm::ElementKind::Node)};
  BakedTile native;
  CHECK(BakeStructures(raw, *heights, mesher, *scratch, native).has_value(),
        "declared generated footprint accepts its existing original point identity");
  raw.Structures[0].SourceId.Id = 8;
  BakedTile missingNative;
  CHECK(!BakeStructures(raw, *heights, mesher, *scratch, missingNative),
        "missing original element cannot silently generate a native product");
  raw.Structures[0].SourceId = {
      .Id = 7, .Kind = static_cast<uint8_t>(outshine::Generators::Osm::ElementKind::Way)};
  BakedTile wrongKind;
  CHECK(!BakeStructures(raw, *heights, mesher, *scratch, wrongKind),
        "matching numeric node ID does not satisfy a way reference");
  raw.SourceInputs = {};
  BakedTile missingOwner;
  CHECK(!BakeStructures(raw, *heights, mesher, *scratch, missingOwner),
        "original identity without its owner cannot fall back to legacy input");
  raw.Structures[0].SourceId = {};
  for (const double invalid : {-std::numeric_limits<double>::infinity(),
                               12.0,
                               13.0,
                               std::numeric_limits<double>::quiet_NaN()}) {
    raw.Structures[0].MinimumHeightM = invalid;
    BakedTile product;
    CHECK(!BakeStructures(raw, *heights, mesher, *scratch, product),
          "nonfinite and inverted intervals cannot generate geometry");
  }
  return Report();
}
