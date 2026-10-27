#include "MvtTransportSnapshot.h"
#include "MvtTransportSnapshotInternal.h"
#include "MvtTransportWireFixture.h"
#include "OsmField.h"
#include "StreetField.h"
#include "StreetGraphBuilder.h"
#include "GroundMaterialCatalog.h"
#include "RegionSurfaceRecipes.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <stop_token>
#include <string>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
namespace Wire = outshine::Test::MvtTransportWire;
constexpr size_t kBudget = 1 << 20;
constexpr std::array<GeographicBounds, 1> kCoverage{
    {{.WestDeg = 0, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 0}}};

void CheckMembers(const MvtTransportSourceFacts &facts) {
  const auto edges = facts.Network->Topology().Edges();
  bool reverse = false;
  bool crossing = false;
  uint64_t endpoints = 0;
  for (size_t at = 0; at < facts.Groups.size(); ++at) {
    const auto &group = facts.Groups[at];
    endpoints += group.OriginalEndpointCount;
    CHECK(group.Complete && group.FilteredEndpointCount == 0 &&
              group.OriginalEndpointCount == group.MemberCount,
          "all fixture physical ends are counted independently of travel direction");
    crossing |= group.MemberCount == 4 && facts.NodeGroups->Groups[at].NodeCount == 2;
    const auto members = std::span(facts.Members).subspan(group.FirstMember, group.MemberCount);
    for (const auto &member : members) {
      const auto edge = std::ranges::find(edges, member.Edge, &World::TransportEdge::Id);
      CHECK(edge != edges.end(), "each member retains an actually published directed edge");
      if (edge == edges.end()) { continue; }
      CHECK(member.NodeGroup == at &&
                (member.End == 0 ? edge->FromNodeId : edge->ToNodeId) == member.DomainNode,
            "directed From/To orientation binds the exact domain node");
      const auto ids = facts.NodeGroups->NodesOf(facts.NodeGroups->Groups[at]);
      CHECK(std::ranges::find(ids, member.DomainNode) != ids.end(),
            "each domain member belongs to its retained original node group");
      const auto path = std::ranges::find(facts.Network->Provenance().Paths,
                                          member.Edge.PathId,
                                          &World::TransportPathSource::PathId);
      CHECK(path != facts.Network->Provenance().Paths.end(), "member has actual path provenance");
      if (path == facts.Network->Provenance().Paths.end()) { continue; }
      const auto &reference = facts.Network->Provenance().References[path->FirstReference];
      CHECK(member.SourceStep == reference.SegmentOrdinal,
            "actual SourceStep is retained separately from final fragment ordinal");
      reverse |= member.Edge.Direction == World::EdgeDirection::Reverse;
      if (facts.NodeGroups->Groups[at].SourceConnectionCount == 0) {
        CHECK(group.OriginalEndpointCount == 1 && group.MemberCount == 1 && group.Complete,
              "a one-way target with zero outgoing raw records still has its physical endpoint");
      }
    }
  }
  CHECK(endpoints == 8 && facts.Members.size() == 8 && reverse && crossing,
        "the real four-span fixture retains eight ends, reverse-only World travel and split "
        "crossing");
}

void CheckOneWayTarget(const OsmField &field, const StreetField &streets) {
  auto network = Path::Network::Create({.CellM = 1}, {.RadiusM = kWgs84A});
  CHECK(network.has_value(), "a controlled native one-way source graph is available");
  if (!network) { return; }
  for (size_t at = 0; at < streets.Ways().size(); ++at) {
    const auto &way = streets.Ways()[at];
    CHECK(network->Lay(field.Points().subspan(static_cast<size_t>(way.FirstPoint) * 2,
                                              static_cast<size_t>(way.PointCount) * 2),
                       {.HalfWidthM = way.HalfWidthM, .Oneway = at == 0, .Tag = at}),
          "the first actual coordinate ring is explicitly one-way in the controlled source");
  }
  std::string error;
  CHECK(network->Weave(error), "the controlled one-way source weaves");
  const auto made = BuildMvtTransportSnapshot(
      field,
      streets,
      *network,
      kCoverage,
      {},
      {.NodeGroupPayloadBytesMost = kBudget, .SourceFactsPayloadBytesMost = kBudget});
  CHECK(made && made->SourceFacts, "the actual one-way source captures");
  if (!made || !made->SourceFacts) { return; }
  bool incoming = false;
  for (size_t at = 0; at < made->SourceFacts->Groups.size(); ++at) {
    if (made->NodeGroups->Groups[at].SourceConnectionCount != 0) { continue; }
    incoming = true;
    const auto &group = made->SourceFacts->Groups[at];
    CHECK(group.Complete && group.OriginalEndpointCount == 1 && group.MemberCount == 1 &&
              group.FilteredEndpointCount == 0,
          "the incoming one-way end counts even though its native outgoing record count is zero");
  }
  CHECK(incoming, "the controlled source really contains a one-way sink");
}

void CheckWays(const MvtTransportSourceFacts &facts,
               const OsmField &field,
               const StreetField &streets) {
  CHECK(facts.Ways.size() == 2, "each actually used source way is captured once");
  for (const auto &way : facts.Ways) {
    const auto &original = streets.Ways()[way.StreetWay];
    const auto source = streets.SourceOf(field, way.StreetWay);
    CHECK(source && source->Feature == way.Feature && source->Ring == way.Ring &&
              original.PointCount == way.PointCount && original.OwnsEnds == way.OwnsEnds,
          "local Way/Feature/Ring/count/frontier words come from the same actual source scope");
    CHECK(std::bit_cast<uint32_t>(original.HalfWidthM) == way.HalfWidthWord &&
              std::bit_cast<uint32_t>(original.MaxGradient) == way.MaximumGradientWord,
          "profile values preserve actual binary32 words rather than rereading JSON doubles");
    const size_t first = original.FirstPoint;
    const size_t last = first + original.PointCount - 1;
    const std::array<size_t, 4> points{first, first + 1, last - 1, last};
    for (size_t end = 0; end < points.size(); ++end) {
      CHECK(
          way.EndStencil[end].LatitudeDeg == field.Points()[2 * points[end]] &&
              way.EndStencil[end].LongitudeDeg == field.Points()[2 * points[end] + 1],
          "both terminal stencils retain original source coordinates without snapped substitution");
    }
  }
}

void CheckFiltered(const OsmField &field,
                   const StreetField &streets,
                   const Path::Network &network) {
  auto sources = MvtTransport::PrepareSources(field, streets, {});
  CHECK(sources && sources->Ways.size() == 2, "actual prepared source rows are available");
  if (!sources || sources->Ways.size() != 2) { return; }
  sources->Ways.front().Usable = false;
  const auto made = MvtTransport::Publish(
      std::move(*sources),
      network,
      field,
      streets,
      kCoverage,
      {},
      {.NodeGroupPayloadBytesMost = kBudget, .SourceFactsPayloadBytesMost = kBudget});
  CHECK(made && made->SourceFacts, "controlled filtering preserves the remaining World graph");
  if (!made || !made->SourceFacts) { return; }
  bool missing = false;
  for (const auto &group : made->SourceFacts->Groups) {
    if (group.FilteredEndpointCount == 0) { continue; }
    missing = true;
    CHECK(!group.Complete && group.OriginalEndpointCount == 4 && group.MemberCount == 2 &&
              group.FilteredEndpointCount == 2,
          "filtered source incidence remains explicit rather than a false complete bridge end");
  }
  CHECK(missing, "the real original crossing exposes its deliberately filtered ground incidences");
}

void CheckBudget(const OsmField &field,
                 const StreetField &streets,
                 const Path::Network &network,
                 const MvtTransportSourceFacts &facts) {
  size_t low = 1;
  size_t high = kBudget;
  for (size_t round = 0; round < 20 && low < high; ++round) {
    const size_t mid = low + (high - low) / 2;
    const auto made = BuildMvtTransportSnapshot(
        field,
        streets,
        network,
        kCoverage,
        {},
        {.NodeGroupPayloadBytesMost = kBudget, .SourceFactsPayloadBytesMost = mid});
    CHECK(made.has_value(), "bounded fact capture never blocks an otherwise valid World");
    if (!made) { return; }
    if (made->SourceFactState == MvtTransportSourceFactState::Captured) {
      high = mid;
    } else {
      CHECK(made->SourceFactState == MvtTransportSourceFactState::BudgetExceeded,
            "insufficient requested payload is explicit");
      low = mid + 1;
    }
  }
  CHECK(low == high && low > 1, "the fixture has one exact finite logical payload threshold");
  for (const size_t budget : {low - 1, low}) {
    const auto made = BuildMvtTransportSnapshot(
        field,
        streets,
        network,
        kCoverage,
        {},
        {.NodeGroupPayloadBytesMost = kBudget, .SourceFactsPayloadBytesMost = budget});
    CHECK(made &&
              ((budget == low && made->SourceFacts && made->SourceFacts->Digest == facts.Digest) ||
               (budget < low && !made->SourceFacts &&
                made->SourceFactState == MvtTransportSourceFactState::BudgetExceeded)),
          "one byte below the exact logical bound refuses facts; the bound retains exact content");
  }
  std::println("MVT_SOURCE_FACT_PAYLOAD threshold={} groups={} members={} ways={}",
               low,
               facts.Groups.size(),
               facts.Members.size(),
               facts.Ways.size());
  std::stop_source stop;
  stop.request_stop();
  CHECK(!BuildMvtTransportSnapshot(
            field,
            streets,
            network,
            kCoverage,
            stop.get_token(),
            {.NodeGroupPayloadBytesMost = kBudget, .SourceFactsPayloadBytesMost = 1}),
        "budget exhaustion never masks requested cancellation");
}

std::shared_ptr<const MvtTransportSourceFacts>
Capture(const Generators::RegionSurfaceRecipes &recipes) {
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  CHECK(field.Accept(Wire::kTileX, Wire::kTileX, Wire::Tile()).has_value(), "actual MVT loads");
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  const auto graph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  CHECK(graph.Graph != nullptr, "existing native source graph builds");
  if (!graph.Graph) { return {}; }
  const auto ordinary = BuildMvtTransportSnapshot(field, streets, *graph.Graph, kCoverage);
  CHECK(ordinary && !ordinary->SourceFacts &&
            ordinary->SourceFactState == MvtTransportSourceFactState::NotRequested,
        "ordinary normalization requests no source facts");
  const auto made = BuildMvtTransportSnapshot(
      field,
      streets,
      *graph.Graph,
      kCoverage,
      {},
      {.NodeGroupPayloadBytesMost = kBudget, .SourceFactsPayloadBytesMost = kBudget});
  CHECK(made && made->SourceFacts && made->SourceFactState == MvtTransportSourceFactState::Captured,
        "explicit budgets capture owned source facts");
  if (!made || !made->SourceFacts || !ordinary) { return {}; }
  const auto &facts = *made->SourceFacts;
  CHECK(facts.Network == made->Network && facts.NodeGroups == made->NodeGroups &&
            facts.SourceGeneration == field.Generation() &&
            facts.SourceWayCount == streets.Ways().size() &&
            facts.Network->SourceIdentity() == ordinary->Network->SourceIdentity(),
        "facts bind the exact immutable normalized snapshot and current source generation");
  CheckMembers(facts);
  CheckOneWayTarget(field, streets);
  CheckWays(facts, field, streets);
  CheckFiltered(field, streets, *graph.Graph);
  CheckBudget(field, streets, *graph.Graph, facts);
  for (const auto request : {MvtTransportCapture{.SourceFactsPayloadBytesMost = kBudget},
                             MvtTransportCapture{.NodeGroupPayloadBytesMost = kBudget,
                                                 .SourceFactsPayloadBytesMost = 1}}) {
    const auto limited =
        BuildMvtTransportSnapshot(field, streets, *graph.Graph, kCoverage, {}, request);
    CHECK(limited && !limited->SourceFacts &&
              limited->Network->SourceIdentity() == ordinary->Network->SourceIdentity() &&
              limited->SourceFactState == (request.NodeGroupPayloadBytesMost == 0
                                               ? MvtTransportSourceFactState::MissingNodeGroups
                                               : MvtTransportSourceFactState::BudgetExceeded),
          "missing partition and insufficient fact budgets preserve ordinary World publication");
  }
  auto changed = facts;
  ++changed.Ways.front().MaximumGradientWord;
  MvtTransport::BindSourceFacts(changed);
  CHECK(changed.Digest != facts.Digest, "candidate binding includes exact profile words");
  changed = facts;
  ++changed.Groups.front().OriginalEndpointCount;
  MvtTransport::BindSourceFacts(changed);
  CHECK(changed.Digest != facts.Digest, "candidate binding includes actual original incidence");
  return made->SourceFacts;
}
}

int main() {
  Generators::GroundMaterialCatalog materials;
  Generators::RegionSurfaceRecipes recipes;
  CHECK(materials.Load("data/world/ground-materials.json") &&
            recipes.Load("data/world/vegetation.json", materials),
        "actual recipes load");
  if (recipes.Ready()) {
    const auto facts = Capture(recipes);
    CHECK(facts && facts->Network->Topology().Nodes().size() == 6 && facts->Ways.size() == 2,
          "captured facts and World data remain owned after all source arrays retire");
  }
  return Report();
}
