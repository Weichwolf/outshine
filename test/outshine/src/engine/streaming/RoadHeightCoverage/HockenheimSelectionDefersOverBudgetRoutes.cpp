#include "HockenheimTransportFixture.h"
#include "HockenheimCircuitFixture.h"
#include "OsmTransportBuilder.h"
#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadHeightCoverage.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace Circuit = outshine::Test::HockenheimFixture;

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  using namespace outshine::World;

  auto fixture = Circuit::LoadNetwork();
  CHECK(fixture,
        fixture ? "pinned source, topology and directed circuit load" : fixture.error().c_str());
  if (!fixture) { return Report(); }
  auto &topology = fixture->Graph;
  auto &circuit = fixture->Route;

  ResolvedTransport transport{.Graph = std::move(topology),
                              .Routes = {{.Id = "grand-prix", .Circuit = circuit},
                                         {.Id = "second-view", .Circuit = std::move(circuit)}}};
  const TransportNetworkSnapshot snapshot(std::move(transport), {}, {});

  const auto selected =
      RoadHeightCoverage::Select(snapshot, {.Zoom = 15, .MaximumEdges = 512, .MaximumTiles = 256});
  CHECK(selected && selected->SelectedRouteIndices == std::vector<size_t>{0} &&
            selected->SelectedEdges == Circuit::EdgeCount && selected->DeferredRoutes == 1 &&
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
