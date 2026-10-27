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
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <stop_token>
#include <string>
#include <tuple>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
namespace Wire = outshine::Test::MvtTransportWire;

void SameWorld(const MvtTransportSnapshot &a, const MvtTransportSnapshot &b) {
  const auto node = [](const World::TransportNode &n) {
    return std::tie(n.SourceNodeId, n.LatitudeDeg, n.LongitudeDeg);
  };
  const auto edge = [](const World::TransportEdge &e) {
    return std::tie(e.Id,
                    e.FromNodeId,
                    e.ToNodeId,
                    e.Modes,
                    e.Facility,
                    e.Surface,
                    e.WidthM,
                    e.LaneCount,
                    e.Layer,
                    e.Bridge,
                    e.Tunnel,
                    e.Access);
  };
  const auto omission = [](const MvtTransportOmission &o) {
    return std::tie(o.Source, o.Feature, o.Reason);
  };
  CHECK(a.Network->SourceIdentity() == b.Network->SourceIdentity() &&
            std::ranges::equal(
                a.Network->Topology().Nodes(), b.Network->Topology().Nodes(), {}, node, node) &&
            std::ranges::equal(
                a.Network->Topology().Edges(), b.Network->Topology().Edges(), {}, edge, edge) &&
            std::ranges::equal(a.Omissions, b.Omissions, {}, omission, omission) &&
            a.StreetWays == b.StreetWays && a.InputConnections == b.InputConnections,
        "optional capture preserves every World node, edge, omission and source mapping");
}

void SourcePartition(const MvtTransportSnapshot &snapshot, const Path::Network &source) {
  const auto &groups = *snapshot.NodeGroups;
  const auto nodes = snapshot.Network->Topology().Nodes();
  std::vector<uint64_t> seen;
  bool split = false;
  bool terminal = false;
  uint64_t previous = 0;
  for (const auto &group : groups.Groups) {
    const auto ids = groups.NodesOf(group);
    CHECK(!ids.empty() && std::ranges::is_sorted(ids) && ids.front() > previous,
          "groups and their members have a canonical nonempty order");
    if (ids.empty()) { continue; }
    previous = ids.front();
    split |= ids.size() > 1;
    terminal |= ids.size() == 1 && group.SourceConnectionCount == 1;
    const auto first = std::ranges::find(nodes, ids.front(), &World::TransportNode::SourceNodeId);
    CHECK(first != nodes.end(), "each group starts with a published domain node");
    if (first == nodes.end()) { continue; }
    size_t matches = 0;
    for (size_t at = 0; at < source.NodeCount(); ++at) {
      const auto position = source.PositionOfNode(at);
      if (!position || position->LatitudeDeg != first->LatitudeDeg ||
          position->LongitudeDeg != first->LongitudeDeg) {
        continue;
      }
      ++matches;
      CHECK(group.SourceConnectionCount == source.ConnectionsFrom(at).size(),
            "the witness retains the actual outgoing source record count");
    }
    CHECK(matches == 1, "this fixture has one unambiguous native node at each tested position");
    for (const auto id : ids) {
      const auto found = std::ranges::find(nodes, id, &World::TransportNode::SourceNodeId);
      CHECK(found != nodes.end() && found->LatitudeDeg == first->LatitudeDeg &&
                found->LongitudeDeg == first->LongitudeDeg,
            "the captured domains retain the common source position");
      seen.push_back(id);
    }
  }
  std::ranges::sort(seen);
  CHECK(split && std::ranges::equal(seen, nodes, {}, {}, &World::TransportNode::SourceNodeId),
        "capture partitions all published nodes once and retains a real split source node");
  CHECK(terminal, "ordinary source ends remain singleton groups with one actual connection");
}

std::shared_ptr<const MvtTransportNodeGroups>
CaptureOrder(const Generators::RegionSurfaceRecipes &recipes, bool reverse) {
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  const int first = reverse ? Wire::kNextTileX : Wire::kTileX;
  const int second = reverse ? Wire::kTileX : Wire::kNextTileX;
  CHECK(field.Accept(first, Wire::kTileX, Wire::Tile()) &&
            field.Accept(second, Wire::kTileX, Wire::Tile()),
        "both native tile arrival orders load");
  StreetField streets;
  for (size_t at = 0; at < 2 && !streets.Ingested(field); ++at) {
    (void)streets.Ingest(field, recipes, {.CentreX = first, .CentreY = Wire::kTileX, .Rings = 1});
  }
  const auto graph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  CHECK(graph.Graph != nullptr, "the reordered actual native topology builds");
  if (!graph.Graph) { return {}; }
  const std::array<GeographicBounds, 1> coverage{
      {{.WestDeg = 0, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 0}}};
  const auto made = BuildMvtTransportSnapshot(
      field, streets, *graph.Graph, coverage, {}, {.NodeGroupPayloadBytesMost = 4096});
  CHECK(made && made->NodeGroups, "the reordered native graph publishes its witness");
  if (!made || !made->NodeGroups) { return {}; }
  SourcePartition(*made, *graph.Graph);
  return made->NodeGroups;
}

void EmptyCapture(const Generators::RegionSurfaceRecipes &recipes) {
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  CHECK(field.Accept(Wire::kTileX, Wire::kTileX, Wire::EmptyTile()).has_value(),
        "an actual empty MVT source loads");
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  const auto graph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  CHECK(graph.Graph != nullptr, "empty source topology builds");
  if (!graph.Graph) { return; }
  const std::array<GeographicBounds, 1> coverage{
      {{.WestDeg = 0, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 0}}};
  const auto made =
      BuildMvtTransportSnapshot(field,
                                streets,
                                *graph.Graph,
                                coverage,
                                {},
                                {.NodeGroupPayloadBytesMost = sizeof(MvtTransportNodeGroups)});
  CHECK(made && made->NodeGroups && made->NodeGroups->Groups.empty() &&
            made->NodeGroups->Nodes.empty() &&
            made->NodeGroupState == MvtTransportNodeGroupState::Captured,
        "an empty captured partition is explicit and fits its exact object payload");
}

void CanonicalBinding() {
  MvtTransportNodeGroups a{.Groups = {{.FirstNode = 0, .NodeCount = 2, .SourceConnectionCount = 4},
                                      {.FirstNode = 2, .NodeCount = 1, .SourceConnectionCount = 1}},
                           .Nodes = {9, 3, 7}};
  MvtTransportNodeGroups b{.Groups = {{.FirstNode = 0, .NodeCount = 1, .SourceConnectionCount = 1},
                                      {.FirstNode = 1, .NodeCount = 2, .SourceConnectionCount = 4}},
                           .Nodes = {7, 3, 9}};
  MvtTransport::CanonicalizeNodeGroups(a, "revision");
  MvtTransport::CanonicalizeNodeGroups(b, "revision");
  CHECK(a.Nodes == b.Nodes && a.Digest == b.Digest,
        "native node and member reordering cannot change the canonical binding");
  auto separated = a;
  separated.Groups = {{.FirstNode = 0, .NodeCount = 1, .SourceConnectionCount = 2},
                      {.FirstNode = 1, .NodeCount = 1, .SourceConnectionCount = 2},
                      {.FirstNode = 2, .NodeCount = 1, .SourceConnectionCount = 1}};
  MvtTransport::CanonicalizeNodeGroups(separated, "revision");
  CHECK(separated.Digest != a.Digest,
        "two distinct original nodes cannot masquerade as one shared original node");
  auto changed = a;
  ++changed.Groups.front().SourceConnectionCount;
  MvtTransport::CanonicalizeNodeGroups(changed, "revision");
  CHECK(changed.Digest != a.Digest, "the binding includes original source incidence");
  MvtTransport::CanonicalizeNodeGroups(changed, "another-revision");
  MvtTransport::CanonicalizeNodeGroups(b, "another-revision");
  CHECK(b.Digest != a.Digest, "the binding includes the actual normalized Network revision");
}

void Capture(const Generators::RegionSurfaceRecipes &recipes) {
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  CHECK(field.Accept(Wire::kTileX, Wire::kTileX, Wire::Tile()).has_value(),
        "the actual MVT fixture loads");
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  const auto graph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  CHECK(graph.Graph != nullptr, "the existing native source network builds");
  if (!graph.Graph) { return; }
  const std::array<GeographicBounds, 1> coverage{
      {{.WestDeg = 0, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 0}}};
  const auto plain = BuildMvtTransportSnapshot(field, streets, *graph.Graph, coverage);
  const auto captured = BuildMvtTransportSnapshot(
      field, streets, *graph.Graph, coverage, {}, {.NodeGroupPayloadBytesMost = 4096});
  CHECK(plain && captured, "both ordinary and captured snapshots publish");
  if (!plain || !captured) { return; }
  CHECK(plain->NodeGroupState == MvtTransportNodeGroupState::NotRequested && !plain->NodeGroups,
        "ordinary callers allocate no node-group table");
  CHECK(captured->NodeGroupState == MvtTransportNodeGroupState::Captured && captured->NodeGroups,
        "an explicitly budgeted caller receives an owned immutable witness");
  if (!captured->NodeGroups) { return; }
  SameWorld(*plain, *captured);
  SourcePartition(*captured, *graph.Graph);
  const auto &groups = *captured->NodeGroups;
  const size_t bytes = sizeof(groups) + groups.Groups.size() * sizeof(MvtTransportNodeGroup) +
                       groups.Nodes.size() * sizeof(uint64_t) * 2;
  for (const size_t budget : {size_t{1}, bytes - 1, bytes}) {
    const auto limited = BuildMvtTransportSnapshot(
        field, streets, *graph.Graph, coverage, {}, {.NodeGroupPayloadBytesMost = budget});
    CHECK(limited.has_value(), "capture budget exhaustion never blocks World publication");
    if (!limited) { continue; }
    SameWorld(*plain, *limited);
    CHECK((budget == bytes && limited->NodeGroups &&
           limited->NodeGroupState == MvtTransportNodeGroupState::Captured &&
           limited->NodeGroups->Digest == groups.Digest) ||
              (budget < bytes && !limited->NodeGroups &&
               limited->NodeGroupState == MvtTransportNodeGroupState::BudgetExceeded),
          "the exact logical object/array/temporary payload threshold is enforced");
  }
  std::stop_source stop;
  stop.request_stop();
  CHECK(!BuildMvtTransportSnapshot(field,
                                   streets,
                                   *graph.Graph,
                                   coverage,
                                   stop.get_token(),
                                   {.NodeGroupPayloadBytesMost = bytes}),
        "capture preserves requested normalization cancellation");
  std::println("MVT_NODE_GROUP_PAYLOAD groups={} nodes={} peak_logical_bytes={}",
               groups.Groups.size(),
               groups.Nodes.size(),
               bytes);
}
}

int main() {
  CanonicalBinding();
  Generators::GroundMaterialCatalog materials;
  Generators::RegionSurfaceRecipes recipes;
  CHECK(materials.Load("data/world/ground-materials.json") &&
            recipes.Load("data/world/vegetation.json", materials),
        "actual transport recipes load");
  if (recipes.Ready()) {
    Capture(recipes);
    EmptyCapture(recipes);
    const auto forward = CaptureOrder(recipes, false);
    const auto reverse = CaptureOrder(recipes, true);
    CHECK(forward && reverse && forward->Nodes == reverse->Nodes &&
              forward->Digest == reverse->Digest,
          "canonical capture survives source destruction and actual tile arrival reordering");
  }
  return Report();
}
