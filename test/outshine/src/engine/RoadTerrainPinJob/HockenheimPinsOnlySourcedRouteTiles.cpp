#include "HockenheimTransportFixture.h"
#include "OsmTransportBuilder.h"
#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadAlignment.h"
#include "RoadTerrainPinJob.h"

#include <cstddef>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Circuit = outshine::Test::HockenheimFixture;

namespace {
constexpr uint64_t kPendingCandidate = 17;
constexpr uint64_t kCompleteCandidate = 18;

std::vector<outshine::Data::TileId> RouteTiles(const outshine::World::TransportTopology &topology,
                                               const outshine::World::CircuitRoute &route,
                                               int zoom) {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  using namespace outshine::World;
  std::vector<TileId> tiles;
  for (const TransportEdgeId id : route.EdgeIds) {
    const TransportEdge *edge = topology.FindEdge(id);
    CHECK(edge != nullptr, "every selected source edge resolves");
    if (edge == nullptr) { return {}; }
    for (const uint64_t nodeId : {edge->FromNodeId, edge->ToNodeId}) {
      const TransportNode *node = topology.FindNode(nodeId);
      CHECK(node != nullptr, "every selected source node resolves");
      if (node == nullptr) { return {}; }
      const Ground::TileSpot spot = Ground::HeightField::SpotOf(
          {.LongitudeDeg = node->LongitudeDeg, .LatitudeDeg = node->LatitudeDeg}, zoom);
      tiles.push_back(
          {.Zoom = zoom, .X = static_cast<uint32_t>(spot.X), .Y = static_cast<uint32_t>(spot.Y)});
    }
  }
  std::ranges::sort(tiles, [](TileId left, TileId right) {
    return std::tie(left.Zoom, left.X, left.Y) < std::tie(right.Zoom, right.X, right.Y);
  });
  tiles.erase(std::ranges::unique(tiles).begin(), tiles.end());
  return tiles;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using namespace outshine::World;

  auto fixture = Circuit::LoadNetwork();
  CHECK(fixture,
        fixture ? "pinned source, topology and directed circuit load" : fixture.error().c_str());
  if (!fixture) { return Report(); }
  const auto &topology = fixture->Graph;
  auto &route = fixture->Route;

  constexpr int zoom = 15;
  const auto tiles = RouteTiles(topology, route, zoom);
  CHECK(tiles.size() > 1 && tiles.size() < 64,
        "the route needs a bounded multi-tile terrain pin at its declared zoom");
  const auto selected = RoadTerrainPinJob::SelectTiles(
      topology, route.SourceIdentity, route.EdgeIds, {.Zoom = zoom, .MaximumTiles = tiles.size()});
  CHECK(selected && *selected == tiles,
        "tile selection equals independently computed source-node coverage");
  const SourceIdentity stale{.DatasetId = "openstreetmap", .Revision = "previous"};
  const auto staleSelection = RoadTerrainPinJob::SelectTiles(
      topology, stale, route.EdgeIds, {.Zoom = zoom, .MaximumTiles = tiles.size()});
  CHECK(!staleSelection && staleSelection.error().Code == RoadTerrainPinErrorCode::SourceMismatch,
        "a stale route revision cannot request terrain from the current graph");

  using Entry = Generators::TerrainFieldSet::Entry;
  std::vector<Entry> fields;
  for (const TileId tile : tiles) {
    auto field = std::make_shared<Ground::TerrainField>(2, 2);
    for (uint32_t row = 0; row < 2; ++row) {
      for (uint32_t col = 0; col < 2; ++col) { field->SetM(row, col, 100.0f); }
    }
    field->AddSource({.Kind = DataKind::Elevation,
                      .Tile = tile,
                      .SourceId = "controlled-dem",
                      .Revision = "r1"});
    fields.emplace_back(tile, std::move(field));
  }

  auto shortBudget = RoadTerrainPinJob::Begin(
      Generators::TerrainFieldSet(fields),
      topology,
      route.SourceIdentity,
      route.EdgeIds,
      {.Zoom = zoom, .MaximumTiles = tiles.size() - 1, .CandidateGeneration = kPendingCandidate});
  CHECK(!shortBudget && shortBudget.error().Code == RoadTerrainPinErrorCode::TooManyTiles,
        "a route wider than the declared tile budget fails with a typed error");

  auto completeFields = fields;
  fields.pop_back();
  auto missing = RoadTerrainPinJob::Begin(
      Generators::TerrainFieldSet(fields),
      topology,
      route.SourceIdentity,
      route.EdgeIds,
      {.Zoom = zoom, .MaximumTiles = tiles.size(), .CandidateGeneration = kPendingCandidate});
  CHECK(missing.has_value(), "the bounded route pin starts from immutable source fields");
  if (!missing) { return Report(); }
  CHECK(!missing->Advance(tiles.size()) && !missing->Complete() &&
            missing->PendingTile() == tiles.back() && missing->CopiedTiles() == tiles.size() - 1,
        "a missing source tile leaves the candidate pending without synthesized elevation");

  auto complete = RoadTerrainPinJob::Begin(
      Generators::TerrainFieldSet(std::move(completeFields)),
      topology,
      route.SourceIdentity,
      route.EdgeIds,
      {.Zoom = zoom, .MaximumTiles = tiles.size(), .CandidateGeneration = kCompleteCandidate});
  CHECK(complete.has_value(), "the complete route pin starts");
  if (!complete) { return Report(); }
  CHECK(!complete->Advance(0) && complete->CopiedTiles() == 0,
        "zero work budget cannot copy a height tile");
  for (size_t copied = 0; copied < tiles.size() && !complete->Complete(); ++copied) {
    (void)complete->Advance(1);
  }
  CHECK(complete->Complete() && complete->CopiedTiles() == tiles.size() && !complete->PendingTile(),
        "one-tile worker slices finish at the declared bound");
  const auto &result = complete->Result();
  CHECK(result.has_value(), "completed pin exposes its qualified result");
  if (!result) { return Report(); }
  const PinnedRoadTerrain &pin = *result;
  CHECK(pin.Matches(kCompleteCandidate, topology.SourceIdentity()) &&
            !pin.Matches(kPendingCandidate, topology.SourceIdentity()) &&
            pin.Heights->Qualified() && pin.Heights->Blocks().size() == tiles.size() &&
            pin.Heights->Sources().size() == tiles.size(),
        "the result records exact OSM, DEM and candidate provenance");
  const auto chain =
      RoadConstraintChain::Build(topology, route.SourceIdentity, route.EdgeIds, *pin.Heights);
  CHECK(chain.has_value(), "all 267 directed route edges sample the pinned terrain");
  if (!chain) { return Report(); }
  const auto alignment = RoadAlignmentBuilder::Build(*chain);
  CHECK(alignment && alignment->Closed() && alignment->Edges().size() == route.EdgeIds.size(),
        "the sourced multi-tile pin feeds one periodic native alignment");
  return Report();
}
