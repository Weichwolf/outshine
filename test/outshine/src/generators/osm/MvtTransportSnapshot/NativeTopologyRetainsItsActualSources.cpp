#include "MvtTransportSnapshot.h"
#include "MvtTransportWireFixture.h"
#include "OsmField.h"
#include "StreetField.h"
#include "StreetGraphBuilder.h"
#include "GroundMaterialCatalog.h"
#include "RegionSurfaceRecipes.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <optional>
#include <memory>
#include <ratio>
#include <print>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;

using outshine::Test::MvtTransportWire::kNextTileX;
using outshine::Test::MvtTransportWire::kTileX;
using outshine::Test::MvtTransportWire::Tile;
constexpr uint32_t kAbsentSegmentOrdinal = 1000;

void CheckSnapshot(const MvtTransportSnapshot &made) {
  using namespace outshine::Test;
  const auto &snapshot = *made.Network;
  CHECK(World::ValidateTransportNetwork(snapshot).has_value(),
        "native topology and provenance validate");
  CHECK(snapshot.SourceIdentity().DatasetId == "outshine/mvt/transport" &&
            !snapshot.SourceIdentity().Revision.empty(),
        "MVT owns an explicit derived identity space");
  CHECK(snapshot.Provenance().IdentitySpace == World::TransportIdentitySpace::DerivedFragments &&
            snapshot.Provenance().Sources.size() == 1,
        "actual input tile is retained once independently of optional provider IDs");
  CHECK(snapshot.Topology().Edges().size() == 6 && snapshot.Topology().Nodes().size() == 6,
        "two source segments each retain their direction and separated crossing domains");
  size_t withoutId = 0;
  size_t zeroId = 0;
  for (const auto &reference : snapshot.Provenance().References) {
    withoutId += static_cast<size_t>(!reference.ProviderElementId);
    zeroId += static_cast<size_t>(reference.ProviderElementId == 0);
    CHECK(reference.Source == 0 && reference.SegmentOrdinal < 2,
          "path provenance retains the actual zero-based source segment ordinal");
  }
  CHECK(withoutId == 2 && zeroId == 2, "absent and zero provider identities stay distinct");
  for (const auto &edge : snapshot.Topology().Edges()) {
    if (edge.Layer == 0) {
      CHECK(edge.Id.Direction == World::EdgeDirection::Reverse &&
                edge.Access == World::TransportAccess::Restricted &&
                edge.Surface == World::TransportSurface::Gravel,
            "source reverse direction, access and surface survive recipe geometry");
    } else {
      CHECK(edge.Layer == 1 && edge.Bridge, "bridge semantics remain native attributes");
    }
    const auto *to = snapshot.Topology().FindNode(edge.ToNodeId);
    CHECK(to != nullptr, "every endpoint is present");
    for (const auto &outgoing : snapshot.Topology().OutgoingFrom(edge.ToNodeId)) {
      const auto &next = snapshot.Topology().Edges()[outgoing.EdgeIndex];
      CHECK(edge.Layer == next.Layer, "XY coincidence never invents a cross-level connection");
    }
  }
  CHECK(std::ranges::count(made.Omissions,
                           MvtTransportOmissionReason::UnsupportedTunnel,
                           &MvtTransportOmission::Reason) == 1 &&
            std::ranges::count(made.Omissions,
                               MvtTransportOmissionReason::UnqualifiedLevelConnection,
                               &MvtTransportOmission::Reason) == 2,
        "unsupported tunnel and both unqualified level connections name their sources");
  std::println("MVT_NATIVE nodes={} directed_edges={} source_paths={} input_connections={} "
               "capacity_bytes={} local_binding_capacity_bytes={} normalization_owner_bytes={}",
               snapshot.Topology().Nodes().size(),
               snapshot.Topology().Edges().size(),
               snapshot.Provenance().Paths.size(),
               made.InputConnections,
               snapshot.CapacityBytes(),
               made.StreetWays.capacity() * sizeof(uint32_t),
               sizeof(MvtTransportSnapshot));
}

std::optional<uint32_t> CheckResolvedWay(const MvtTransportSnapshot &made,
                                         const OsmField &field,
                                         const StreetField &streets,
                                         World::TransportEdgeId edge) {
  using namespace outshine::Test;
  const auto selected = ResolveMvtTransportWays(made, field, streets, std::span(&edge, 1));
  CHECK(selected && selected->size() == 1,
        "the same native edge resolves after either actual tile arrival order");
  if (!selected || selected->size() != 1) { return {}; }
  const auto source = streets.SourceOf(field, selected->front());
  CHECK(source && streets.Ways()[selected->front()].Bridge ==
                      made.Network->Topology().FindEdge(edge)->Bridge,
        "the local demand retains the requested source semantics");
  return selected->front();
}

void CheckArrivalOrder(const Generators::RegionSurfaceRecipes &recipes,
                       std::span<const GeographicBounds> coverage) {
  using namespace outshine::Test;
  std::array<std::shared_ptr<const World::TransportNetworkSnapshot>, 2> outputs;
  std::array<uint32_t, 2> selectedWays{};
  World::TransportEdgeId requested{};
  const std::array<std::string, 1> layers{"streets"};
  for (size_t order = 0; order < outputs.size(); ++order) {
    OsmField field(16, layers);
    const bool first = field.Accept(order == 0 ? kTileX : kNextTileX, kTileX, Tile()).has_value();
    const bool second = field.Accept(order == 0 ? kNextTileX : kTileX, kTileX, Tile()).has_value();
    CHECK(first && second, "both original tile arrival orders are accepted");
    StreetField streets;
    for (size_t tile = 0; tile < 2 && !streets.Ingested(field); ++tile) {
      (void)streets.Ingest(
          field,
          recipes,
          {.CentreX = order == 0 ? kTileX : kNextTileX, .CentreY = kTileX, .Rings = 1});
    }
    const auto graph = StreetGraphBuilder::BuildOneShot(
        streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
    CHECK(graph.Graph != nullptr, "existing topology publishes for either source order");
    if (!graph.Graph) { return; }
    const auto made = BuildMvtTransportSnapshot(field, streets, *graph.Graph, coverage);
    CHECK(made.has_value(),
          made ? "arrival order publishes native topology" : made.error().c_str());
    if (!made) { return; }
    outputs[order] = made->Network;
    if (order == 0) { requested = made->Network->Topology().Edges().front().Id; }
    const auto selected = CheckResolvedWay(*made, field, streets, requested);
    if (!selected) { return; }
    selectedWays[order] = *selected;
  }
  CHECK(outputs[0]->SourceIdentity() == outputs[1]->SourceIdentity(),
        "canonical full-product identity is independent of input arrival order");
  CHECK(std::ranges::equal(outputs[0]->Topology().Nodes(),
                           outputs[1]->Topology().Nodes(),
                           {},
                           &World::TransportNode::SourceNodeId,
                           &World::TransportNode::SourceNodeId) &&
            std::ranges::equal(outputs[0]->Topology().Edges(),
                               outputs[1]->Topology().Edges(),
                               {},
                               &World::TransportEdge::Id,
                               &World::TransportEdge::Id),
        "all derived node and path identities remain stable with repeated cross-tile provider IDs");
  CHECK(selectedWays[0] != selectedWays[1],
        "stable native identity selects different local ordinals when arrival order changes");
}

void CheckDemand(const MvtTransportSnapshot &made,
                 const OsmField &field,
                 const StreetField &streets) {
  using namespace outshine::Test;
  std::vector<World::TransportEdgeId> edges;
  for (const auto &edge : made.Network->Topology().Edges()) { edges.push_back(edge.Id); }
  BeginCppAllocationProbe();
  const auto selected = ResolveMvtTransportWays(made, field, streets, edges);
  const auto allocations = EndCppAllocationProbe();
  std::println("MVT_DEMAND input_edges={} allocation_calls={} requested_bytes={}",
               edges.size(),
               allocations.Calls,
               allocations.Bytes);
  CHECK(selected && *selected == std::vector<uint32_t>({0, 1}),
        "directed fragments coalesce to the two original source ways");
  const auto empty = ResolveMvtTransportWays(made, field, streets, {});
  CHECK(empty && empty->empty(), "empty native demand selects no source ways");
  auto absent = edges.front();
  absent.SegmentOrdinal = kAbsentSegmentOrdinal;
  CHECK(!ResolveMvtTransportWays(made, field, streets, std::span(&absent, 1)),
        "a valid path cannot admit an absent directed fragment");
  const auto retired = streets.Without(0);
  CHECK(!ResolveMvtTransportWays(made, field, retired, edges),
        "retired local source ways cannot become functional demand");
  std::stop_source canceled;
  canceled.request_stop();
  CHECK(!ResolveMvtTransportWays(made, field, streets, {}, canceled.get_token()),
        "cancellation also rejects empty native demand");
  const std::array<std::string, 1> layers{"streets"};
  OsmField unrelated(16, layers);
  CHECK(unrelated.Accept(kTileX, kTileX, Tile()).has_value(),
        "equal source bytes have another owner");
  const StreetField otherStreets;
  CHECK(!ResolveMvtTransportWays(made, unrelated, streets, edges) &&
            !ResolveMvtTransportWays(made, field, otherStreets, edges),
        "neither equal foreign bytes nor another street field can reuse local bindings");
}

void CheckEmptyRegion(const Generators::RegionSurfaceRecipes &recipes,
                      std::span<const GeographicBounds> coverage) {
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  CHECK(field.Accept(kTileX, kTileX, MvtTransportWire::EmptyTile()).has_value(),
        "an actual MVT layer may declare a region with no transport features");
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  const auto graph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  CHECK(graph.Graph && streets.Ways().empty(), "the source builder retains its valid empty graph");
  if (!graph.Graph) { return; }
  const auto made = BuildMvtTransportSnapshot(field, streets, *graph.Graph, coverage);
  CHECK(made && made->Network->Topology().Nodes().empty() &&
            made->Network->Topology().Edges().empty() && made->Omissions.empty() &&
            made->Network->Provenance().Sources.size() == 1,
        "known empty transport retains actual source provenance without invented omissions");
  if (!made) { return; }
  const auto selected = ResolveMvtTransportWays(*made, field, streets, {});
  CHECK(selected && selected->empty(), "empty functional demand resolves without a source rebuild");
}

void CheckPartialTopology(const OsmField &field,
                          const StreetField &streets,
                          std::span<const GeographicBounds> coverage) {
  using namespace outshine::Test;
  const auto partial = streets.Without(0);
  const auto graph = StreetGraphBuilder::BuildOneShot(
      partial, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  CHECK(graph.Graph && graph.Graph->WayCount() == 1,
        "the actual source builder can retain a valid partial network");
  if (!graph.Graph) { return; }
  const auto made = BuildMvtTransportSnapshot(field, streets, *graph.Graph, coverage);
  CHECK(made && made->Network->Topology().Edges().size() == 4,
        "native normalization preserves the valid directed bridge instead of rejecting the region");
  if (!made) { return; }
  const auto omitted = streets.SourceOf(field, 0);
  CHECK(omitted &&
            std::ranges::count(made->Omissions,
                               MvtTransportOmissionReason::MissingTopology,
                               &MvtTransportOmission::Reason) == 1 &&
            std::ranges::any_of(made->Omissions,
                                [&](const MvtTransportOmission &one) {
                                  return one.Reason ==
                                             MvtTransportOmissionReason::MissingTopology &&
                                         one.Feature ==
                                             field.Features()[omitted->Feature].ContentDigest;
                                }),
        "the absent usable source retains its actual feature identity and omission reason");
  CHECK(std::ranges::all_of(made->Network->Topology().Edges(),
                            [](const auto &edge) { return edge.Bridge; }),
        "the missing source is recorded without inventing replacement geometry");
}
}

int main() {
  using namespace outshine::Test;
  Generators::GroundMaterialCatalog materials;
  Generators::RegionSurfaceRecipes recipes;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            recipes.Load("src/assets/world/vegetation.json", materials),
        "actual transport recipes load");
  if (!recipes.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  CHECK(field.Accept(kTileX, kTileX, Tile()).has_value(), "real MVT source is accepted");
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  CHECK(streets.Ways().size() == 2 && streets.TunnelCount() == 1,
        "existing normalization retains the usable ways and counts its tunnel omission");
  const auto graph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  CHECK(graph.Graph != nullptr, "the existing source topology is solved once");
  if (!graph.Graph) { return Report(); }
  const std::array<GeographicBounds, 1> coverage{
      {{.WestDeg = 0, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 0}}};
  BeginCppAllocationProbe();
  const auto began = std::chrono::steady_clock::now();
  const auto made = BuildMvtTransportSnapshot(field, streets, *graph.Graph, coverage);
  const auto allocations = EndCppAllocationProbe();
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  std::println("MVT_NATIVE_COST allocation_calls={} requested_bytes={} normalization_ms={}",
               allocations.Calls,
               allocations.Bytes,
               elapsedMs);
  CHECK(made.has_value(), made ? "native snapshot publishes" : made.error().c_str());
  if (made) {
    CheckSnapshot(*made);
    CheckDemand(*made, field, streets);
  }
  CheckPartialTopology(field, streets, coverage);
  CheckEmptyRegion(recipes, coverage);
  std::stop_source cancelled;
  cancelled.request_stop();
  CHECK(!BuildMvtTransportSnapshot(field, streets, *graph.Graph, coverage, cancelled.get_token()),
        "cancellation returns without publishing a snapshot");
  OsmField unrelated(16, layers);
  CHECK(unrelated.Accept(kTileX, kTileX, Tile()).has_value(),
        "equal bytes load into another owner");
  CHECK(!BuildMvtTransportSnapshot(unrelated, streets, *graph.Graph, coverage),
        "equal local ordinals cannot bind another source owner");
  CheckArrivalOrder(recipes, coverage);
  CHECK(field.Accept(kNextTileX, kTileX, Tile()).has_value(),
        "another source tile changes the generation");
  CHECK(made && !ResolveMvtTransportWays(*made, field, streets, {}),
        "even empty demand cannot reuse a binding after its source generation changes");
  return Report();
}
