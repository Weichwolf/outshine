#include "MvtTransportSnapshot.h"
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

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
namespace Wire = outshine::Test::MvtTransportWire;

Wire::Bytes SplitTile(bool reverse) {
  Wire::Bytes layer{Wire::kLayerVersion, 2, Wire::kLayerExtent, 64};
  Wire::Text(layer, 0x0a, "streets");
  Wire::Text(layer, Wire::kLayerKey, "kind");
  Wire::Text(layer, Wire::kLayerKey, "oneway");
  for (const auto *value : {"path", "-1"}) {
    Wire::Bytes encoded;
    Wire::Text(encoded, 0x0a, value);
    Wire::Message(layer, Wire::kLayerValue, encoded);
  }
  Wire::Bytes tags{0, 0};
  if (reverse) { tags.insert(tags.end(), {1, 1}); }
  Wire::Message(layer,
                Wire::kFeatureTags,
                Wire::Feature({.Id = 1, .Tags = tags, .Geometry = {9, 0, 32, 10, 64, 0}}));
  Wire::Message(layer,
                Wire::kFeatureTags,
                Wire::Feature({.Id = 2, .Tags = {0, 0}, .Geometry = {9, 32, 32, 10, 0, 32}}));
  Wire::Bytes out;
  Wire::Message(out, Wire::kLayerKey, layer);
  return out;
}

void CheckFacts(const MvtTransportSourceFacts &facts, bool reverse) {
  bool inner = false;
  bool outer = false;
  bool fragment = false;
  for (const auto &group : facts.Groups) {
    const auto members = std::span(facts.Members).subspan(group.FirstMember, group.MemberCount);
    if (group.MemberCount == 3) {
      inner = true;
      CHECK(group.Complete && group.OriginalEndpointCount == 3 && !group.Frontier,
            "an inner three-ended node never inherits the outer source frontier");
    }
    for (const auto &member : members) {
      if (member.StreetWay != 0) { continue; }
      CHECK(member.SourceStep == 0 &&
                member.Edge.Direction ==
                    (reverse ? World::EdgeDirection::Reverse : World::EdgeDirection::Forward),
            "both fragments retain the same original step and actual travel direction");
      fragment |= member.Edge.SegmentOrdinal > 0;
      if (group.MemberCount == 3) {
        CHECK(!member.SourceTerminal, "split fragment ends are not original source endpoints");
      }
      const size_t physical = member.End ^ static_cast<uint8_t>(member.Edge.Direction);
      if (member.SourceTerminal && physical == 0) {
        outer = true;
        CHECK(group.Frontier, "the actual unowned outer source endpoint retains its frontier");
      }
    }
  }
  CHECK(inner && outer && fragment,
        "the control actually covers inner, outer and split ordinal cases");
}

void CheckSplit(const Generators::RegionSurfaceRecipes &recipes, bool reverse) {
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  CHECK(field.Accept(Wire::kTileX, Wire::kTileX, SplitTile(reverse)).has_value(),
        "the actual fragmented first/last-segment source tile loads");
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  CHECK(streets.Ways().size() == 2 && !streets.Ways()[0].OwnsEnds[0] &&
            streets.Ways()[0].OwnsEnds[1],
        "only the first original source endpoint is unowned");
  if (streets.Ways().size() != 2) { return; }
  auto network = Path::Network::Create({.CellM = 1}, {.RadiusM = kWgs84A});
  CHECK(network && !network->NodeOfWayPoint(0, 0), "unwoven point identity is unavailable");
  if (!network) { return; }
  for (size_t at = 0; at < streets.Ways().size(); ++at) {
    const auto &way = streets.Ways()[at];
    CHECK(network->Lay(field.Points().subspan(static_cast<size_t>(way.FirstPoint) * 2,
                                              static_cast<size_t>(way.PointCount) * 2),
                       {.HalfWidthM = way.HalfWidthM, .Tag = at}),
          "actual source coordinates lay");
  }
  std::string error;
  CHECK(network->Weave(error) && network->NodeCount() == 4 && network->EdgeCount() == 6,
        "Weave splits the original two-point source at the actual T-junction");
  CHECK(!network->NodeOfWayPoint(network->WayCount(), 0) && !network->NodeOfWayPoint(0, 2),
        "original point lookup bounds are explicit");
  for (size_t at = 0; at < network->WayCount(); ++at) {
    CHECK(network->NodeOfWayPoint(at, 0) && network->NodeOfWayPoint(at, 1),
          "original point identities survive native Way reordering and fragmentation");
  }
  constexpr std::array<GeographicBounds, 1> coverage{
      {{.WestDeg = 0, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 0}}};
  constexpr size_t budget = 1 << 20;
  const auto made = BuildMvtTransportSnapshot(
      field,
      streets,
      *network,
      coverage,
      {},
      {.NodeGroupPayloadBytesMost = budget, .SourceFactsPayloadBytesMost = budget});
  CHECK(made && made->SourceFacts, "the actual split source captures");
  if (!made || !made->SourceFacts) { return; }
  CheckFacts(*made->SourceFacts, reverse);
}
}

int main() {
  Generators::GroundMaterialCatalog materials;
  Generators::RegionSurfaceRecipes recipes;
  CHECK(materials.Load("data/world/ground-materials.json") &&
            recipes.Load("data/world/vegetation.json", materials),
        "actual source recipes load");
  if (recipes.Ready()) {
    CheckSplit(recipes, false);
    CheckSplit(recipes, true);
  }
  return Report();
}
