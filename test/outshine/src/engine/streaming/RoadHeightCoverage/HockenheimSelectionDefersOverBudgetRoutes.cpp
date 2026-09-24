#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadHeightCoverage.h"

#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  using namespace outshine::World;

  std::ifstream input("src/assets/world/osm/HockenheimringGrandPrix.osm", std::ios::binary);
  CHECK(input.good(), "the pinned circuit source exists");
  if (!input) { return Report(); }
  const std::string xml(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  auto source = OsmXmlReader::Read(xml, {.DatasetId = "openstreetmap", .Revision = "pin-r1"});
  CHECK(source.has_value(), "the source parses");
  if (!source) { return Report(); }
  auto topology = TransportTopology::Build(*source);
  CHECK(topology.has_value(), "the source builds a topology");
  if (!topology) { return Report(); }
  auto circuit = topology->ResolveCircuit(*source, 284588);
  CHECK(circuit.has_value(), "the 267-edge route resolves");
  if (!circuit) { return Report(); }
  ResolvedTransport transport{.Graph = std::move(*topology),
                              .Routes = {{.Id = "grand-prix", .Circuit = *circuit},
                                         {.Id = "second-view", .Circuit = std::move(*circuit)}}};
  TransportNetworkSnapshot snapshot(std::move(*source), std::move(transport), {}, {});

  const auto selected =
      RoadHeightCoverage::Select(snapshot, {.Zoom = 15, .MaximumEdges = 512, .MaximumTiles = 256});
  CHECK(selected && selected->SelectedRouteIndices == std::vector<size_t>{0} &&
            selected->SelectedEdges == 267 && selected->DeferredRoutes == 1 &&
            selected->Tiles.size() > 1 && selected->Tiles.size() < 64,
        "one local circuit is admitted while the second exceeds the edge budget");
  const auto edgeLimited =
      RoadHeightCoverage::Select(snapshot, {.Zoom = 15, .MaximumEdges = 100, .MaximumTiles = 256});
  CHECK(edgeLimited && edgeLimited->Tiles.empty() && edgeLimited->SelectedRouteIndices.empty() &&
            edgeLimited->DeferredRoutes == 2,
        "overlong routes defer without failing the ground candidate");
  const auto tileLimited =
      RoadHeightCoverage::Select(snapshot, {.Zoom = 15, .MaximumEdges = 512, .MaximumTiles = 1});
  CHECK(tileLimited && tileLimited->Tiles.empty() && tileLimited->SelectedRouteIndices.empty() &&
            tileLimited->DeferredRoutes == 2,
        "a route exceeding local DEM coverage defers without partial source tiles");
  const auto routeLimited = RoadHeightCoverage::Select(
      snapshot, {.Zoom = 15, .MaximumEdges = 600, .MaximumTiles = 256, .MaximumRoutes = 1});
  CHECK(routeLimited && routeLimited->SelectedRouteIndices == std::vector<size_t>{0} &&
            routeLimited->DeferredRoutes == 1,
        "route count is bounded independently of the edge and terrain budgets");
  return Report();
}
