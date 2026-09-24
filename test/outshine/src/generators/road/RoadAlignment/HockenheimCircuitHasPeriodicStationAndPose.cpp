#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadAlignment.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using namespace outshine::World;

  std::ifstream input("src/assets/world/osm/HockenheimringGrandPrix.osm", std::ios::binary);
  CHECK(input.good(), "the pinned Hockenheim source is available");
  if (!input) { return Report(); }
  const std::string xml(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  const auto source = OsmXmlReader::Read(xml, {.DatasetId = "openstreetmap", .Revision = "pin-r1"});
  CHECK(source.has_value(), "Hockenheim source parses");
  if (!source) { return Report(); }
  const auto topology = TransportTopology::Build(*source);
  CHECK(topology.has_value(), "Hockenheim source builds the logical topology");
  if (!topology) { return Report(); }
  const auto route = topology->ResolveCircuit(*source, 284588);
  CHECK(route.has_value(), "the main circuit resolves by source relation");
  if (!route) { return Report(); }

  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100.0f, 100.0f, 100.0f, 100.0f};
  block.Sources = {{.Kind = DataKind::Elevation,
                    .Tile = {.Zoom = 0, .X = 0, .Y = 0},
                    .SourceId = "analytic-dem",
                    .Revision = "r1"}};
  const auto heights = Ground::HeightField::Of(0, {block});
  const auto constraints =
      RoadConstraintChain::Build(*topology, route->SourceIdentity, route->EdgeIds, *heights);
  CHECK(constraints.has_value(), "all route edges have pinned terrain constraints");
  if (!constraints) { return Report(); }
  const auto alignment = RoadAlignmentBuilder::Build(*constraints);
  CHECK(alignment.has_value(), "the closed source circuit builds a periodic road centerline");
  if (!alignment) { return Report(); }
  CHECK(alignment->Closed() && alignment->Edges().size() == 267 && alignment->LengthM() > 4500.0 &&
            alignment->LengthM() < 4700.0,
        "the source circuit retains every edge and a plausible measured arc length");

  bool allSeams = true;
  double maximumSourceOffsetM = 0.0;
  for (size_t index = 0; index < alignment->Edges().size(); ++index) {
    const RoadAlignmentEdge &current = alignment->Edges()[index];
    const RoadAlignmentEdge &next = alignment->Edges()[(index + 1) % alignment->Edges().size()];
    const auto end =
        alignment->AtEdgeStation(current.SourceEdge, current.EndStationM - current.StartStationM);
    const auto start = alignment->AtEdgeStation(next.SourceEdge, 0.0);
    allSeams &= current.SourceEdge == route->EdgeIds[index] &&
                alignment->FindEdge(current.SourceEdge) != nullptr && end && start &&
                std::abs(end->PositionM.EastM - start->PositionM.EastM) < 1e-7 &&
                std::abs(end->PositionM.NorthM - start->PositionM.NorthM) < 1e-7 &&
                std::abs(end->PositionM.UpM - start->PositionM.UpM) < 1e-7 &&
                Dot(end->TangentEnu, start->TangentEnu) > 1.0 - 1e-10 &&
                std::abs(end->WidthM - start->WidthM) < 1e-10 &&
                current.EndStationM > current.StartStationM;
    const EastNorthUp &from = constraints->Points()[index].TerrainLocalM;
    const EastNorthUp &to = constraints->Points()[index + 1].TerrainLocalM;
    const double eastM = to.EastM - from.EastM;
    const double northM = to.NorthM - from.NorthM;
    const double chordM = std::hypot(eastM, northM);
    for (int sample = 1; sample < 8; ++sample) {
      const double edgeStationM =
          (current.EndStationM - current.StartStationM) * static_cast<double>(sample) / 8.0;
      const auto pose = alignment->AtEdgeStation(current.SourceEdge, edgeStationM);
      if (!pose) {
        allSeams = false;
        continue;
      }
      const double offsetM = std::abs((pose->PositionM.EastM - from.EastM) * northM -
                                      (pose->PositionM.NorthM - from.NorthM) * eastM) /
                             chordM;
      maximumSourceOffsetM = std::max(maximumSourceOffsetM, offsetM);
    }
  }
  CHECK(allSeams, "all 267 edge seams, including lap closure, share pose and width");
  CHECK(maximumSourceOffsetM < 1.0,
        "the smoothed centerline stays within one metre of the independent source chords");

  const auto first = alignment->AtStation(0.0);
  const auto lap = alignment->AtStation(alignment->LengthM());
  CHECK(first && lap && first->SourceEdge == lap->SourceEdge &&
            first->PositionM == lap->PositionM && first->TangentEnu.Axis == lap->TangentEnu.Axis,
        "one full station period returns to the identical source pose");
  bool continuousMotion = true;
  auto previous = first;
  for (double stationM = 1.0; stationM < alignment->LengthM(); stationM += 1.0) {
    const auto current = alignment->AtStation(stationM);
    continuousMotion &= previous && current &&
                        std::hypot(current->PositionM.EastM - previous->PositionM.EastM,
                                   current->PositionM.NorthM - previous->PositionM.NorthM,
                                   current->PositionM.UpM - previous->PositionM.UpM) < 1.1;
    previous = current;
  }
  CHECK(continuousMotion, "metre-spaced camera samples contain no route-edge teleport");
  return Report();
}
