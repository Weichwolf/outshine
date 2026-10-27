#include "MvtTransportSnapshotInternal.h"
#include "MvtTransportWireFixture.h"
#include "OsmField.h"
#include "StreetField.h"
#include "GroundMaterialCatalog.h"
#include "RegionSurfaceRecipes.h"
#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
namespace Wire = outshine::Test::MvtTransportWire;

Wire::Bytes EndContactTile() {
  Wire::Bytes layer{Wire::kLayerVersion, 2, Wire::kLayerExtent, 64};
  Wire::Text(layer, 0x0a, "streets");
  for (const auto *key : {"kind", "layer", "bridge"}) { Wire::Text(layer, Wire::kLayerKey, key); }
  for (const auto *text : {"path", "0", "1", "yes"}) {
    Wire::Bytes value;
    Wire::Text(value, 0x0a, text);
    Wire::Message(layer, Wire::kLayerValue, value);
  }
  Wire::Message(layer,
                Wire::kFeatureTags,
                Wire::Feature({.Id = 1, .Tags = {0, 0, 1, 1}, .Geometry = {9, 16, 32, 10, 32, 0}}));
  Wire::Message(
      layer,
      Wire::kFeatureTags,
      Wire::Feature({.Id = 2, .Tags = {0, 0, 1, 2, 2, 3}, .Geometry = {9, 48, 32, 10, 32, 0}}));
  Wire::Message(layer,
                Wire::kFeatureTags,
                Wire::Feature({.Id = 3, .Tags = {0, 0, 1, 1}, .Geometry = {9, 48, 0, 10, 0, 32}}));
  Wire::Bytes tile;
  Wire::Message(tile, Wire::kLayerKey, layer);
  return tile;
}
}

int main() {
  Generators::GroundMaterialCatalog materials;
  Generators::RegionSurfaceRecipes recipes;
  CHECK(materials.Load("data/world/ground-materials.json") &&
            recipes.Load("data/world/vegetation.json", materials),
        "actual recipes load");
  if (!recipes.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  CHECK(field.Accept(Wire::kTileX, Wire::kTileX, EndContactTile()).has_value(),
        "the controlled three-way native MVT tile loads");
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  CHECK(streets.Ways().size() == 3, "all three actual source rings are ingested");
  if (streets.Ways().size() != 3) { return Report(); }
  auto network = Path::Network::Create({.CellM = 1}, {.RadiusM = kWgs84A});
  CHECK(network.has_value(), "the controlled original source graph is available");
  if (!network) { return Report(); }
  for (size_t at = 0; at < streets.Ways().size(); ++at) {
    const auto &way = streets.Ways()[at];
    CHECK(network->Lay(field.Points().subspan(static_cast<size_t>(way.FirstPoint) * 2,
                                              static_cast<size_t>(way.PointCount) * 2),
                       {.HalfWidthM = way.HalfWidthM, .Oneway = at == 2, .Tag = at}),
          "the third physical source ring has only its incoming one-way record");
  }
  std::string error;
  CHECK(network->Weave(error), "the three-ring original graph weaves");
  auto sources = MvtTransport::PrepareSources(field, streets, {});
  CHECK(sources.has_value(), "actual normalized source rows are available");
  if (!sources) { return Report(); }
  sources->Ways[2].Usable = false;
  constexpr std::array<GeographicBounds, 1> coverage{
      {{.WestDeg = 0, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 0}}};
  constexpr size_t budget = 1 << 20;
  const auto made = MvtTransport::Publish(
      std::move(*sources),
      *network,
      field,
      streets,
      coverage,
      {},
      {.NodeGroupPayloadBytesMost = budget, .SourceFactsPayloadBytesMost = budget});
  CHECK(made && made->SourceFacts, "the filtered World graph publishes its original source facts");
  if (!made || !made->SourceFacts) { return Report(); }
  const auto &facts = *made->SourceFacts;
  CHECK(facts.Members.size() == 4 && facts.Ways.size() == 2 &&
            facts.Network->Topology().Edges().size() == 4,
        "the omitted third source is not silently reintroduced into World or used Way records");
  bool found = false;
  for (size_t at = 0; at < facts.Groups.size(); ++at) {
    const auto &group = facts.Groups[at];
    if (group.MemberCount != 2) { continue; }
    found = true;
    CHECK(facts.NodeGroups->Groups[at].NodeCount == 2 &&
              facts.NodeGroups->Groups[at].SourceConnectionCount == 2,
          "the original outgoing count deceptively equals the two published domain members");
    CHECK(group.OriginalEndpointCount == 3 && group.FilteredEndpointCount == 1 && !group.Complete,
          "the extra filtered incoming endpoint prevents false source completeness");
    const auto members = std::span(facts.Members).subspan(group.FirstMember, group.MemberCount);
    CHECK(members[0].DomainNode != members[1].DomainNode && members[0].StreetWay < 2 &&
              members[1].StreetWay < 2,
          "separated ground and bridge members retain their actual source identities");
  }
  CHECK(found, "the controlled source contains the exact misleading outgoing-count counterexample");
  return Report();
}
