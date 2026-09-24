#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadAlignment.h"

#include <array>
#include <cmath>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using namespace outshine::World;

  constexpr std::string_view xml = "<osm version='0.6'>"
                                   "<node id='1' lat='0.01' lon='0.01'/>"
                                   "<node id='2' lat='0.01' lon='0.0101'/>"
                                   "<node id='3' lat='0.01005' lon='0.0102'/>"
                                   "<node id='4' lat='0.0101' lon='0.0103'/>"
                                   "<way id='10'><nd ref='1'/><nd ref='2'/>"
                                   "<tag k='highway' v='primary'/><tag k='oneway' v='yes'/>"
                                   "<tag k='surface' v='asphalt'/><tag k='width' v='6'/></way>"
                                   "<way id='20'><nd ref='2'/><nd ref='3'/>"
                                   "<tag k='highway' v='primary'/><tag k='oneway' v='yes'/>"
                                   "<tag k='surface' v='asphalt'/><tag k='width' v='8'/></way>"
                                   "<way id='30'><nd ref='3'/><nd ref='4'/>"
                                   "<tag k='highway' v='primary'/><tag k='oneway' v='yes'/>"
                                   "<tag k='bridge' v='yes'/></way></osm>";
  const auto source = OsmXmlReader::Read(xml, {.DatasetId = "analytic", .Revision = "r1"});
  CHECK(source.has_value(), "the analytic road source parses");
  if (!source) { return Report(); }
  const auto topology = TransportTopology::Build(*source);
  CHECK(topology.has_value(), "the analytic road source builds topology");
  if (!topology) { return Report(); }

  Ground::HeightField::Block block;
  block.At = Ground::HeightField::SpotOf({.LongitudeDeg = 0.01, .LatitudeDeg = 0.01}, 14);
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100.0f, 200.0f, 100.0f, 200.0f};
  block.Sources = {{.Kind = DataKind::Elevation,
                    .Tile = {.Zoom = 14,
                             .X = static_cast<uint32_t>(block.At.X),
                             .Y = static_cast<uint32_t>(block.At.Y)},
                    .SourceId = "analytic-dem",
                    .Revision = "r1"}};
  const auto heights = Ground::HeightField::Of(14, {block});
  const TransportEdgeId first{
      .WayId = 10, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward};
  const TransportEdgeId second{
      .WayId = 20, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward};
  const std::array route{first, second};
  const auto constraints =
      RoadConstraintChain::Build(*topology, source->SourceIdentity(), route, *heights);
  CHECK(constraints.has_value(), "both selected edges have pinned terrain constraints");
  if (!constraints) { return Report(); }
  const auto alignment = RoadAlignmentBuilder::Build(*constraints);
  CHECK(alignment.has_value(), "grounded source edges build a native C1 alignment");
  if (!alignment) { return Report(); }

  CHECK(alignment->Edges().size() == 2 && !alignment->Closed() &&
            alignment->LengthM() > constraints->EstimatedLengthM() &&
            alignment->TerrainDigest() == heights->RasterDigest(),
        "curved 3D arc length exceeds chord sum and retains DEM provenance");
  const RoadAlignmentEdge *firstEdge = alignment->FindEdge(first);
  const RoadAlignmentEdge *secondEdge = alignment->FindEdge(second);
  CHECK(firstEdge && secondEdge && firstEdge->SourceWidthM == 6.0 &&
            secondEdge->SourceWidthM == 8.0 && firstEdge->EndStationM == secondEdge->StartStationM,
        "each source edge maps to one monotonic interval and keeps its declared width");
  if (!firstEdge || !secondEdge) { return Report(); }
  const auto left = alignment->AtEdgeStation(first, firstEdge->EndStationM);
  const auto right = alignment->AtEdgeStation(second, 0.0);
  CHECK(left && right, "both sides of the OSM seam are queryable by source edge ID");
  if (!left || !right) { return Report(); }
  CHECK(std::abs(left->PositionM.EastM - right->PositionM.EastM) < 1e-8 &&
            std::abs(left->PositionM.NorthM - right->PositionM.NorthM) < 1e-8 &&
            std::abs(left->PositionM.UpM - right->PositionM.UpM) < 1e-8 &&
            Dot(left->TangentEnu, right->TangentEnu) > 1.0 - 1e-10 &&
            std::abs(left->WidthM - 7.0) < 1e-10 && std::abs(right->WidthM - 7.0) < 1e-10,
        "position, tangent and width are continuous at the logical join");
  CHECK(alignment->AtStation(firstEdge->EndStationM) &&
            alignment->AtStation(firstEdge->EndStationM)->SourceEdge == second &&
            !alignment->AtStation(-1.0) && !alignment->AtStation(alignment->LengthM() + 1.0),
        "global station advances to the next edge and rejects open-chain overshoot");
  double independentlyMeasuredM = 0.0;
  auto previous = alignment->AtStation(0.0);
  for (double stationM = 0.25; stationM < alignment->LengthM(); stationM += 0.25) {
    const auto current = alignment->AtStation(stationM);
    if (!previous || !current) { break; }
    independentlyMeasuredM += std::hypot(current->PositionM.EastM - previous->PositionM.EastM,
                                         current->PositionM.NorthM - previous->PositionM.NorthM,
                                         current->PositionM.UpM - previous->PositionM.UpM);
    previous = current;
  }
  const auto endPose = alignment->AtStation(alignment->LengthM());
  if (previous && endPose) {
    independentlyMeasuredM += std::hypot(endPose->PositionM.EastM - previous->PositionM.EastM,
                                         endPose->PositionM.NorthM - previous->PositionM.NorthM,
                                         endPose->PositionM.UpM - previous->PositionM.UpM);
  }
  CHECK(endPose && std::abs(independentlyMeasuredM - alignment->LengthM()) < 0.01,
        "reported station length matches an independent dense pose integration");

  const TransportEdgeId bridge{
      .WayId = 30, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward};
  const std::array bridgeRoute{bridge};
  const auto bridgeConstraints =
      RoadConstraintChain::Build(*topology, source->SourceIdentity(), bridgeRoute, *heights);
  CHECK(bridgeConstraints.has_value(), "bridge source retains its terrain and structure intent");
  if (bridgeConstraints) {
    const auto unresolved = RoadAlignmentBuilder::Build(*bridgeConstraints);
    CHECK(!unresolved && unresolved.error().Code == RoadAlignmentErrorCode::StructureNeedsSolver &&
              unresolved.error().SourceEdge == bridge,
          "a bridge cannot silently become a terrain-draped road deck");
  }
  return Report();
}
