#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadConstraintChain.h"

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
  CHECK(source.has_value(), "the Hockenheim source parses");
  if (!source) { return Report(); }
  const auto topology = TransportTopology::Build(*source);
  CHECK(topology.has_value(), "Hockenheim builds the native topology");
  if (!topology) { return Report(); }
  const auto route = topology->ResolveCircuit(*source, 284588);
  CHECK(route.has_value(), "the main circuit resolves by relation ID");
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
  const auto chain =
      RoadConstraintChain::Build(*topology, route->SourceIdentity, route->EdgeIds, *heights);
  CHECK(chain.has_value(), "every selected route edge has a DEM-backed constraint point");
  if (!chain) { return Report(); }
  CHECK(chain->Closed() && chain->Edges().size() == 267 && chain->Points().size() == 268 &&
            chain->Points().front().SourceNodeId == chain->Points().back().SourceNodeId,
        "one ordered, closed chain retains all 267 directed source edges");
  CHECK(chain->EstimatedLengthM() > 3000.0 && chain->EstimatedLengthM() < 7000.0 &&
            std::isfinite(chain->EstimatedLengthM()),
        "the local chord chain has a plausible circuit-scale length");
  bool allRaceway = true;
  for (size_t index = 0; index < chain->Edges().size(); ++index) {
    const RoadConstraintEdge &edge = chain->Edges()[index];
    allRaceway &= edge.SourceEdge == route->EdgeIds[index] &&
                  edge.Facility == TransportFacility::Raceway &&
                  edge.Surface == TransportSurface::Asphalt && edge.WidthM == 12.0 &&
                  !edge.Bridge && !edge.Tunnel;
  }
  CHECK(allRaceway, "the route retains source order and raceway profiles without pitlane edges");
  return Report();
}
