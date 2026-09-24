#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadAlignment.h"
#include "RoadTerrainPinJob.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using namespace outshine::World;

  std::ifstream input("src/assets/world/osm/HockenheimringGrandPrix.osm", std::ios::binary);
  CHECK(input.good(), "the pinned route source exists");
  if (!input) { return Report(); }
  const std::string xml(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  const auto source = OsmXmlReader::Read(xml, {.DatasetId = "openstreetmap", .Revision = "pin-r1"});
  CHECK(source.has_value(), "the route source parses");
  if (!source) { return Report(); }
  const auto topology = TransportTopology::Build(*source);
  CHECK(topology.has_value(), "the source builds a logical topology");
  if (!topology) { return Report(); }
  const auto route = topology->ResolveCircuit(*source, 284588);
  CHECK(route.has_value(), "the circuit retains directed source edges");
  if (!route) { return Report(); }

  constexpr int zoom = 15;
  std::vector<TileId> tiles;
  for (const TransportEdgeId id : route->EdgeIds) {
    const TransportEdge *edge = topology->FindEdge(id);
    CHECK(edge != nullptr, "every selected source edge resolves");
    if (!edge) { return Report(); }
    for (const uint64_t nodeId : {edge->FromNodeId, edge->ToNodeId}) {
      const TransportNode *node = topology->FindNode(nodeId);
      CHECK(node != nullptr, "every selected source node resolves");
      if (!node) { return Report(); }
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
  CHECK(tiles.size() > 1 && tiles.size() < 64,
        "the route needs a bounded multi-tile terrain pin at its declared zoom");

  using Entry = SourcedTerrainFields::Entry;
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
      SourcedTerrainFields(fields),
      *topology,
      route->EdgeIds,
      {.Zoom = zoom, .MaximumTiles = tiles.size() - 1, .CandidateGeneration = 17});
  CHECK(!shortBudget && shortBudget.error().Code == RoadTerrainPinErrorCode::TooManyTiles,
        "a route wider than the declared tile budget fails with a typed error");

  auto completeFields = fields;
  fields.pop_back();
  auto missing = RoadTerrainPinJob::Begin(
      SourcedTerrainFields(fields),
      *topology,
      route->EdgeIds,
      {.Zoom = zoom, .MaximumTiles = tiles.size(), .CandidateGeneration = 17});
  CHECK(missing.has_value(), "the bounded route pin starts from immutable source fields");
  if (!missing) { return Report(); }
  CHECK(!missing->Advance(tiles.size()) && !missing->Complete() &&
            missing->PendingTile() == tiles.back() && missing->CopiedTiles() == tiles.size() - 1,
        "a missing source tile leaves the candidate pending without synthesized elevation");

  auto complete = RoadTerrainPinJob::Begin(
      SourcedTerrainFields(std::move(completeFields)),
      *topology,
      route->EdgeIds,
      {.Zoom = zoom, .MaximumTiles = tiles.size(), .CandidateGeneration = 18});
  CHECK(complete.has_value(), "the complete route pin starts");
  if (!complete) { return Report(); }
  CHECK(!complete->Advance(0) && complete->CopiedTiles() == 0,
        "zero work budget cannot copy a height tile");
  while (!complete->Advance(1)) {}
  CHECK(complete->Complete() && complete->CopiedTiles() == tiles.size() && !complete->PendingTile(),
        "one-tile worker slices finish at the declared bound");
  const PinnedRoadTerrain &pin = *complete->Result();
  CHECK(pin.Matches(18, topology->SourceIdentity()) &&
            !pin.Matches(17, topology->SourceIdentity()) && pin.Heights->Qualified() &&
            pin.Heights->Blocks().size() == tiles.size() &&
            pin.Heights->Sources().size() == tiles.size(),
        "the result records exact OSM, DEM and candidate provenance");
  const auto chain =
      RoadConstraintChain::Build(*topology, route->SourceIdentity, route->EdgeIds, *pin.Heights);
  CHECK(chain.has_value(), "all 267 directed route edges sample the pinned terrain");
  if (!chain) { return Report(); }
  const auto alignment = RoadAlignmentBuilder::Build(*chain);
  CHECK(alignment && alignment->Closed() && alignment->Edges().size() == route->EdgeIds.size(),
        "the sourced multi-tile pin feeds one periodic native alignment");
  return Report();
}
