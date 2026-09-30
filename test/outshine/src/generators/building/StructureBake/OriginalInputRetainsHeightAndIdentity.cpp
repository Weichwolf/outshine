#include "OriginalStructureInput.h"
#include "BuildingMesh.h"
#include "Geodesy.h"
#include "OsmXmlReader.h"
#include "Check.h"

#include <memory>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  auto parsed = Data::OsmXmlReader::Read(
      R"(<osm version="0.6">
      <node id="1" lat="0" lon="0"/><node id="2" lat="0" lon="0.001"/>
      <node id="3" lat="0.001" lon="0.001"/><node id="4" lat="0.001" lon="0"/>
      <way id="10"><nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="4"/><nd ref="1"/>
      <tag k="building:part" v="yes"/><tag k="min_height" v="5.5"/></way>
      </osm>)",
      {.DatasetId = "native-input", .Revision = "one"});
  CHECK(parsed.has_value(), "original raised part parses");
  if (!parsed) { return Report(); }
  auto source = std::make_shared<const Data::OsmSourceSnapshot>(
      Data::OsmSourceSnapshot{.Elements = std::move(*parsed), .Coverage = {}});
  const auto footprints = Ground::OsmBuildingFootprints::Build(source, 4);
  CHECK(footprints.has_value(), "original raised part has a closed footprint");
  if (!footprints) { return Report(); }
  const StructureOriginalSource original{
      .Snapshot = source, .Bounds = {.WestDeg = -1, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1}};
  const OriginalStructurePolicy policy{.Heights = {.StoreyHeightM = 3, .BodyHeightM = 4},
                                       .PointsMost = 4};
  auto raw = OriginalStructureInput(*footprints, original, policy);
  CHECK(raw && raw->Structures.size() == 1, "native input produces one building part");
  if (!raw || raw->Structures.size() != 1) { return Report(); }
  const auto &part = raw->Structures.front();
  CHECK(part.MinimumHeightM == 5.5 && part.HeightM == 9.5 &&
            part.HeightOrigin == Ground::OsmHeightOrigin::Policy,
        "missing top uses an explicit four metre body above original clearance");
  CHECK(part.OriginalId.Kind == Data::OsmElementKind::Way && part.OriginalId.Id == 10 &&
            raw->Original.Snapshot == source,
        "worker input retains typed identity and the complete original source");
  CHECK(part.PointCount == 4 && raw->LatLon.size() == 8 && part.HoleCount == 0,
        "source footprint reaches the generator without duplicated closing point");
  raw->TileSpanM = 1000;
  raw->RequestedDetail = LevelOfDetail::Fine;
  GeoToEcef({.LongitudeDeg = 0, .LatitudeDeg = 0, .HeightM = 100}, raw->AnchorEcef);
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 100);
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights != nullptr, "independent flat DEM is available");
  if (!heights) { return Report(); }
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  BakedTile product;
  const auto baked = BakeStructures(*raw, *heights, mesher, *scratch, product);
  CHECK(baked && product.Prints.size() == 1 && product.Prints.front().HeightM == 9.5 &&
            product.Prints.front().MinimumHeightM == 5.5 && product.DefaultHeights == 1 &&
            product.OsmHeights == 0,
        "original height interval reaches actual mesh generation with derived provenance");
  const auto axes = EnuAxesEcef({.LongitudeDeg = 0, .LatitudeDeg = 0});
  double lower = std::numeric_limits<double>::infinity();
  for (const auto &vertex : product.Built.WallCorners) {
    double up = 0;
    for (int axis = 0; axis < 3; ++axis) { up += vertex.pos[axis] * axes.Up[axis]; }
    lower = std::min(lower, up);
  }
  CHECK(std::isfinite(lower) && std::abs(lower - 5.5) < 0.02,
        "generated surfaces preserve the original clearance above DEM");
  auto limited = policy;
  limited.PointsMost = 3;
  const auto rejected = OriginalStructureInput(*footprints, original, limited);
  CHECK(!rejected && rejected.error() == OriginalStructureInputError::PointBudgetExceeded,
        "coordinate budget is enforced before materializing input");
  const auto unowned = OriginalStructureInput(*footprints, {}, policy);
  CHECK(!unowned && unowned.error() == OriginalStructureInputError::SourceMismatch,
        "geometry cannot be published without its original source owner");
  auto pointParsed = Data::OsmXmlReader::Read(
      R"(<osm version="0.6"><node id="20" lat="0" lon="0">
      <tag k="building" v="toilets"/><tag k="building:levels" v="1"/>
      </node></osm>)",
      {.DatasetId = "native-input", .Revision = "point"});
  CHECK(pointParsed.has_value(), "point building parses");
  if (!pointParsed) { return Report(); }
  const auto pointSource = std::make_shared<const Data::OsmSourceSnapshot>(
      Data::OsmSourceSnapshot{.Elements = std::move(*pointParsed), .Coverage = {}});
  const auto pointFootprints = Ground::OsmBuildingFootprints::Build(pointSource, 1);
  CHECK(pointFootprints.has_value(), "point building preserves its original position");
  if (!pointFootprints) { return Report(); }
  auto pointOriginal = original;
  pointOriginal.Snapshot = pointSource;
  auto pointPolicy = policy;
  pointPolicy.PointsMost = 5;
  const auto noWidth = OriginalStructureInput(*pointFootprints, pointOriginal, pointPolicy);
  CHECK(!noWidth && noWidth.error() == OriginalStructureInputError::InvalidPointPolicy,
        "point geometry requires an explicit generator width");
  pointPolicy.PointWidthM = 4;
  const auto pointRaw = OriginalStructureInput(*pointFootprints, pointOriginal, pointPolicy);
  CHECK(pointRaw && pointRaw->Structures.size() == 1 && pointRaw->LatLon.size() == 10 &&
            pointRaw->Structures.front().OriginalId.Kind == Data::OsmElementKind::Node &&
            pointRaw->Structures.front().HeightOrigin == Ground::OsmHeightOrigin::Levels,
        "generated point footprint retains node identity and level-derived height");
  pointPolicy.PointsMost = 4;
  const auto pointLimited = OriginalStructureInput(*pointFootprints, pointOriginal, pointPolicy);
  CHECK(!pointLimited && pointLimited.error() == OriginalStructureInputError::PointBudgetExceeded,
        "generated corners and retained source point share the coordinate budget");
  return Report();
}
