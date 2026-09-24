#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadAlignment.h"

#include <array>
#include <cmath>
#include <string_view>

namespace {

double Grade(const outshine::Vec3 &direction) {
  return direction[2] / std::hypot(direction[0], direction[1]);
}

outshine::Vec3 Position(const outshine::Generators::RoadConstraintPoint &point) {
  return {{point.TerrainLocalM.EastM, point.TerrainLocalM.NorthM, point.TerrainLocalM.UpM}};
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using namespace outshine::World;

  constexpr std::string_view xml =
      "<osm version='0.6'>"
      "<node id='1' lat='0.01' lon='0.01'/>"
      "<node id='2' lat='0.01' lon='0.0106'/>"
      "<node id='3' lat='0.01001' lon='0.01061'/>"
      "<node id='4' lat='0.0101' lon='0.0107'/>"
      "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/><nd ref='4'/>"
      "<tag k='highway' v='primary'/><tag k='width' v='6'/></way></osm>";
  const auto source = OsmXmlReader::Read(xml, {.DatasetId = "nonuniform", .Revision = "r1"});
  CHECK(source.has_value(), "the nonuniform graded road source parses");
  if (!source) { return Report(); }
  const auto topology = TransportTopology::Build(*source);
  CHECK(topology.has_value(), "the nonuniform graded road has motor topology");
  if (!topology) { return Report(); }

  Ground::HeightField::Block block;
  block.At = Ground::HeightField::SpotOf({.LongitudeDeg = 0.01, .LatitudeDeg = 0.01}, 14);
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100.0f, 100.0f, 200.0f, 200.0f};
  block.Sources = {{.Kind = DataKind::Elevation,
                    .Tile = {.Zoom = 14,
                             .X = static_cast<uint32_t>(block.At.X),
                             .Y = static_cast<uint32_t>(block.At.Y)},
                    .SourceId = "analytic-dem",
                    .Revision = "r1"}};
  const auto heights = Ground::HeightField::Of(14, {block});
  const std::array forward{
      TransportEdgeId{.WayId = 10, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward},
      TransportEdgeId{.WayId = 10, .SegmentOrdinal = 1, .Direction = EdgeDirection::Forward},
      TransportEdgeId{.WayId = 10, .SegmentOrdinal = 2, .Direction = EdgeDirection::Forward}};
  const std::array reverse{
      TransportEdgeId{.WayId = 10, .SegmentOrdinal = 2, .Direction = EdgeDirection::Reverse},
      TransportEdgeId{.WayId = 10, .SegmentOrdinal = 1, .Direction = EdgeDirection::Reverse},
      TransportEdgeId{.WayId = 10, .SegmentOrdinal = 0, .Direction = EdgeDirection::Reverse}};

  for (const auto &route : {forward, reverse}) {
    const auto constraints =
        RoadConstraintChain::Build(*topology, source->SourceIdentity(), route, *heights);
    CHECK(constraints && constraints->Points().size() == 4 && constraints->Edges().size() == 3,
          "source and terrain form three connected graded edges");
    if (!constraints) { continue; }
    const auto alignment = RoadAlignmentBuilder::Build(*constraints);
    CHECK(alignment && !alignment->Closed() && alignment->Edges().size() == 3,
          "unequal source spacing builds one open native alignment");
    if (!alignment) { continue; }
    const auto edges = alignment->Edges();
    const auto points = constraints->Points();
    CHECK(edges[0].SourceEdge == route[0] && edges[1].SourceEdge == route[1] &&
              edges[2].SourceEdge == route[2] && edges[0].EndStationM == edges[1].StartStationM &&
              edges[1].EndStationM == edges[2].StartStationM && edges[0].StartStationM == 0.0 &&
              edges[2].EndStationM == alignment->LengthM(),
          "source IDs and stations remain ordered in both directions");
    for (size_t index = 1; index < edges.size(); ++index) {
      const auto left = alignment->AtEdgeStation(
          route[index - 1], edges[index - 1].EndStationM - edges[index - 1].StartStationM);
      const auto right = alignment->AtEdgeStation(route[index], 0.0);
      CHECK(left && right, "both sides of a graded source seam remain queryable");
      if (!left || !right) { continue; }
      const Vec3 sourcePosition = Position(points[index]);
      CHECK(std::abs(left->PositionM.EastM - sourcePosition[0]) < 1e-7 &&
                std::abs(left->PositionM.NorthM - sourcePosition[1]) < 1e-7 &&
                std::abs(left->PositionM.UpM - sourcePosition[2]) < 1e-7 &&
                Dot(left->TangentEnu, right->TangentEnu) > 1.0 - 1e-10,
            "the centerline keeps its source node and a continuous seam tangent");
    }
    const bool forwardDirection = route[0].Direction == EdgeDirection::Forward;
    const size_t transition = forwardDirection ? 1u : 2u;
    const auto seam = alignment->AtEdgeStation(route[transition - 1],
                                               edges[transition - 1].EndStationM -
                                                   edges[transition - 1].StartStationM);
    CHECK(seam.has_value(), "the short-edge grade transition has a pose");
    if (!seam) { continue; }
    const double incomingGrade =
        Grade(Position(points[transition]) - Position(points[transition - 1]));
    const double outgoingGrade =
        Grade(Position(points[transition + 1]) - Position(points[transition]));
    const double seamGrade = Grade(seam->TangentEnu);
    CHECK(std::abs(incomingGrade - outgoingGrade) > 0.01,
          "the analytic DEM produces a meaningful grade transition");
    const double shortGrade = forwardDirection ? outgoingGrade : incomingGrade;
    CHECK(std::abs(seamGrade - shortGrade) < 0.25 * std::abs(incomingGrade - outgoingGrade),
          "the short edge does not absorb half of the long edge's grade transition");
  }

  constexpr std::string_view equalXml =
      "<osm version='0.6'>"
      "<node id='11' lat='0.01' lon='0.01'/>"
      "<node id='12' lat='0.01' lon='0.0101'/>"
      "<node id='13' lat='0.01007' lon='0.01017'/>"
      "<way id='20'><nd ref='11'/><nd ref='12'/><nd ref='13'/>"
      "<tag k='highway' v='primary'/><tag k='width' v='6'/></way></osm>";
  const auto equalSource =
      OsmXmlReader::Read(equalXml, {.DatasetId = "equal-spacing", .Revision = "r1"});
  CHECK(equalSource.has_value(), "the near-equal-spaced road source parses");
  if (!equalSource) { return Report(); }
  const auto equalTopology = TransportTopology::Build(*equalSource);
  CHECK(equalTopology.has_value(), "the near-equal-spaced road has motor topology");
  if (!equalTopology) { return Report(); }
  const std::array equalRoute{
      TransportEdgeId{.WayId = 20, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward},
      TransportEdgeId{.WayId = 20, .SegmentOrdinal = 1, .Direction = EdgeDirection::Forward}};
  const auto equalConstraints = RoadConstraintChain::Build(
      *equalTopology, equalSource->SourceIdentity(), equalRoute, *heights);
  CHECK(equalConstraints.has_value(), "both near-equal source chords have DEM constraints");
  if (!equalConstraints) { return Report(); }
  const auto equalAlignment = RoadAlignmentBuilder::Build(*equalConstraints);
  CHECK(equalAlignment.has_value(), "near-equal source chords build a native alignment");
  if (!equalAlignment) { return Report(); }
  const auto equalPoints = equalConstraints->Points();
  Vec3 equalIncoming = Position(equalPoints[1]) - Position(equalPoints[0]);
  Vec3 equalOutgoing = Position(equalPoints[2]) - Position(equalPoints[1]);
  CHECK(std::abs(Length(equalIncoming) - Length(equalOutgoing)) < 0.02 * Length(equalIncoming),
        "the control source chords differ in length by less than two percent");
  const bool normalized = Normalise(equalIncoming) && Normalise(equalOutgoing);
  const auto equalSeam =
      equalAlignment->AtEdgeStation(equalRoute[0], equalAlignment->Edges()[0].EndStationM);
  CHECK(normalized && equalSeam &&
            std::abs(Grade(equalSeam->TangentEnu) - Grade(equalIncoming + equalOutgoing)) < 0.001,
        "near-equal chord spacing retains the ordinary bisector grade");
  return Report();
}
