#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadConstraintChain.h"

#include <array>
#include <cmath>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using namespace outshine::World;

  constexpr std::string_view xml =
      "<osm version='0.6'>"
      "<node id='1' lat='0' lon='0'/><node id='2' lat='0' lon='0.0001'/>"
      "<node id='3' lat='0' lon='0.0002'/><node id='4' lat='0' lon='0.0001'/>"
      "<node id='5' lat='0.0001' lon='0.0002'/>"
      "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/>"
      "<tag k='highway' v='raceway'/><tag k='oneway' v='yes'/>"
      "<tag k='surface' v='asphalt'/><tag k='width' v='12'/></way>"
      "<way id='20'><nd ref='4'/><nd ref='5'/>"
      "<tag k='highway' v='primary'/><tag k='oneway' v='yes'/>"
      "<tag k='bridge' v='yes'/></way>"
      "<way id='30'><nd ref='1'/><nd ref='2'/>"
      "<tag k='highway' v='footway'/></way></osm>";
  const auto source = OsmXmlReader::Read(xml, {.DatasetId = "analytic", .Revision = "r1"});
  CHECK(source.has_value(), "the analytic source parses");
  if (!source) { return Report(); }
  const auto topology = TransportTopology::Build(*source);
  CHECK(topology.has_value(), "the analytic source builds transport topology");
  if (!topology) { return Report(); }

  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100.0f, 100.0f, 100.0f, 100.0f};
  block.Sources = {{.Kind = DataKind::Elevation,
                    .Tile = {.Zoom = 0, .X = 0, .Y = 0},
                    .SourceId = "analytic-dem",
                    .Revision = "r1"}};
  const auto heights = Ground::HeightField::Of(0, {block});
  const std::array<TransportEdgeId, 2> main{
      {{.WayId = 10, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward},
       {.WayId = 10, .SegmentOrdinal = 1, .Direction = EdgeDirection::Forward}}};
  const auto chain =
      RoadConstraintChain::Build(*topology, source->SourceIdentity(), main, *heights);
  CHECK(chain.has_value(), "one selected OSM way becomes an ordered native constraint chain");
  if (!chain) { return Report(); }
  CHECK(chain->Edges().size() == 2 && chain->Points().size() == 3 && !chain->Closed() &&
            chain->Edges()[0].SourceEdge == main[0] && chain->Edges()[1].SourceEdge == main[1] &&
            chain->Points()[1].SourceNodeId == 2,
        "edge and node IDs survive the geographic-to-local conversion");
  CHECK(chain->Edges()[0].WidthM == 12.0 &&
            chain->Edges()[0].Surface == TransportSurface::Asphalt &&
            chain->TerrainDigest() == heights->RasterDigest() &&
            chain->TerrainSources().size() == 1 && chain->EstimatedLengthM() > 20.0 &&
            chain->EstimatedLengthM() < 25.0,
        "width, surface, DEM revision and measured local length remain native values");
  bool finiteGround = true;
  for (const RoadConstraintPoint &point : chain->Points()) {
    finiteGround &= point.TerrainElevationM == 100.0 && std::isfinite(point.TerrainLocalM.EastM) &&
                    std::isfinite(point.TerrainLocalM.NorthM) &&
                    std::isfinite(point.TerrainLocalM.UpM);
  }
  CHECK(finiteGround, "ground samples are finite and keep their source height");

  const TransportEdgeId bridgeId{
      .WayId = 20, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward};
  const std::array bridge{bridgeId};
  const auto bridgeChain =
      RoadConstraintChain::Build(*topology, source->SourceIdentity(), bridge, *heights);
  CHECK(bridgeChain && bridgeChain->Edges()[0].Bridge && !bridgeChain->Edges()[0].Tunnel &&
            bridgeChain->Points()[0].SourceNodeId == 4,
        "a bridge carries ground constraints and structure intent without inventing a deck");

  const std::array crossing{main[0], bridgeId};
  const auto disconnected =
      RoadConstraintChain::Build(*topology, source->SourceIdentity(), crossing, *heights);
  CHECK(!disconnected && disconnected.error().Code == RoadConstraintErrorCode::DisconnectedEdge &&
            disconnected.error().SourceNodeId == 4,
        "coincident XY positions with distinct OSM node IDs never create a road join");
  const std::array duplicate{main[0], main[0]};
  const auto repeated =
      RoadConstraintChain::Build(*topology, source->SourceIdentity(), duplicate, *heights);
  CHECK(!repeated && repeated.error().Code == RoadConstraintErrorCode::DuplicateEdge,
        "duplicate source edges are rejected before station construction");
  auto stale = source->SourceIdentity();
  stale.Revision = "r2";
  const auto wrongRevision = RoadConstraintChain::Build(*topology, stale, main, *heights);
  CHECK(!wrongRevision && wrongRevision.error().Code == RoadConstraintErrorCode::SourceMismatch,
        "a selected edge revision cannot be paired with another topology revision");
  const std::array reverse{
      TransportEdgeId{.WayId = 10, .SegmentOrdinal = 0, .Direction = EdgeDirection::Reverse}};
  const auto wrongDirection =
      RoadConstraintChain::Build(*topology, source->SourceIdentity(), reverse, *heights);
  CHECK(!wrongDirection && wrongDirection.error().Code == RoadConstraintErrorCode::MissingEdge,
        "a one-way source edge has no invented reverse geometry");
  const std::array footway{
      TransportEdgeId{.WayId = 30, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward}};
  const auto nonMotor =
      RoadConstraintChain::Build(*topology, source->SourceIdentity(), footway, *heights);
  CHECK(!nonMotor && nonMotor.error().Code == RoadConstraintErrorCode::UnusableEdge,
        "a walking edge cannot enter a motor road constraint chain");
  const auto fallback = Ground::HeightField::Of(0, {block}, true);
  const auto unqualified =
      RoadConstraintChain::Build(*topology, source->SourceIdentity(), main, *fallback);
  CHECK(!unqualified && unqualified.error().Code == RoadConstraintErrorCode::UnqualifiedTerrain,
        "fallback DEM cannot masquerade as a pinned source revision");
  const auto missing = Ground::HeightField::Of(0, {});
  const auto noDem =
      RoadConstraintChain::Build(*topology, source->SourceIdentity(), main, *missing);
  CHECK(!noDem && noDem.error().Code == RoadConstraintErrorCode::MissingTerrain &&
            noDem.error().SourceNodeId == 1,
        "an unavailable DEM sample identifies its source node");
  return Report();
}
