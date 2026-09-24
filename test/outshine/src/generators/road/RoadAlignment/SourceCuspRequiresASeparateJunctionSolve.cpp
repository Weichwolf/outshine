#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadAlignment.h"

#include <array>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using namespace outshine::World;

  constexpr std::string_view xml =
      "<osm version='0.6'>"
      "<node id='1' lat='0' lon='0'/>"
      "<node id='2' lat='0' lon='0.0001'/>"
      "<node id='3' lat='0.000001' lon='0.00002'/>"
      "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/>"
      "<tag k='highway' v='service'/><tag k='oneway' v='yes'/></way></osm>";
  const auto source = OsmXmlReader::Read(xml, {.DatasetId = "cusp", .Revision = "r1"});
  CHECK(source.has_value(), "the source with a sharp turn parses");
  if (!source) { return Report(); }
  const auto topology = TransportTopology::Build(*source);
  CHECK(topology.has_value(), "a connected logical way may still have impossible geometry");
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
  const std::array route{
      TransportEdgeId{.WayId = 10, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward},
      TransportEdgeId{.WayId = 10, .SegmentOrdinal = 1, .Direction = EdgeDirection::Forward}};
  const auto constraints =
      RoadConstraintChain::Build(*topology, source->SourceIdentity(), route, *heights);
  CHECK(constraints.has_value(), "the source IDs and DEM samples form a connected chain");
  if (!constraints) { return Report(); }
  const auto alignment = RoadAlignmentBuilder::Build(*constraints);
  CHECK(!alignment && alignment.error().Code == RoadAlignmentErrorCode::SharpTurn &&
            alignment.error().SourceNodeId == 2,
        "a near reversal is not smoothed into a falsely driveable road");
  return Report();
}
