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
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
namespace Wire = outshine::Test::MvtTransportWire;
constexpr size_t kBudget = 1 << 20;

void CheckPartitions(const OsmField &field,
                     const StreetField &streets,
                     const Path::Network &network,
                     const MvtTransportSnapshot &made) {
  auto sources = MvtTransport::PrepareSources(field, streets, {});
  CHECK(sources.has_value(), "the unchanged source rows prepare");
  if (!sources) { return; }
  auto connections = MvtTransport::ReadConnections(*sources, network, {});
  CHECK(connections.has_value(), "the actual source connections are available");
  if (!connections) { return; }
  MvtTransport::NodeGroupCapture groups;
  groups.BytesMost = kBudget;
  const auto nodes =
      MvtTransport::AssignNodes(*sources, connections->Segments, network, groups, {});
  CHECK(nodes.has_value(), "native connection endpoints receive their actual domain IDs");
  if (!nodes) { return; }
  const auto edges = made.Network->Topology().Edges();
  for (const auto &segment : connections->Segments) {
    CHECK(segment.FragmentOrdinal == 0 &&
              std::ranges::any_of(
                  edges,
                  [&segment](const auto &edge) {
                    return edge.Id.PathId == MvtTransport::Identifier(segment.Key) &&
                           edge.Id.SegmentOrdinal == segment.FragmentOrdinal &&
                           ((edge.FromNodeId == segment.FromId && edge.ToNodeId == segment.ToId) ||
                            (edge.FromNodeId == segment.ToId && edge.ToNodeId == segment.FromId));
                  }),
          "the fixture has exactly the original producer's unsplit fragment identities");
  }
  sources->StreetWays = made.StreetWays;
  sources->Provenance = made.Network->Provenance();
  sources->Omissions = made.Omissions;
  const auto capture = [&](std::shared_ptr<const MvtTransportNodeGroups> partition) {
    return MvtTransport::CaptureSourceFacts(*sources,
                                            connections->Segments,
                                            network,
                                            field,
                                            streets,
                                            made.Network,
                                            std::move(partition),
                                            kBudget,
                                            {});
  };
  const auto valid = capture(made.NodeGroups);
  CHECK(valid && valid->Data && valid->Data->Digest == made.SourceFacts->Digest,
        "complete native input reproduces the unchanged full producer's source-fact digest");
  const auto reject = [&](auto change) {
    auto partition = std::make_shared<MvtTransportNodeGroups>(*made.NodeGroups);
    change(*partition);
    const auto value = capture(std::move(partition));
    CHECK(!value && value.error() == "MVT source facts lost their native node partition",
          "an invalid partition fails explicitly before publishing Captured facts");
  };
  reject([](auto &partition) { partition.Groups.erase(partition.Groups.begin() + 1); });
  reject([](auto &partition) { partition.Groups.pop_back(); });
  reject([](auto &partition) {
    partition.Groups.clear();
    partition.Nodes.clear();
  });
  reject([](auto &partition) {
    partition.Groups.front().FirstNode = static_cast<uint32_t>(partition.Nodes.size());
  });
  reject([](auto &partition) { partition.Groups.front().NodeCount = 0; });
  reject([](auto &partition) {
    partition.Groups.front().NodeCount = static_cast<uint32_t>(partition.Nodes.size()) + 1;
  });
  reject([](auto &partition) { std::swap(partition.Groups[0], partition.Groups[1]); });
  reject([](auto &partition) { partition.Groups[1] = partition.Groups[0]; });
  reject([](auto &partition) { partition.Nodes.front() = 0; });
  const auto segment = connections->Segments.front();
  connections->Segments.front().From = network.NodeCount();
  CHECK(!capture(made.NodeGroups), "an invalid native endpoint fails before indexing the mapping");
  connections->Segments.front() = segment;
  connections->Segments.front().FromId = 0;
  CHECK(!capture(made.NodeGroups), "a missing domain endpoint cannot become a group index");
  connections->Segments.front() = segment;
  const auto restored = capture(made.NodeGroups);
  CHECK(restored && restored->Data && restored->Data->Digest == made.SourceFacts->Digest,
        "negative captures leave the original producer and partition unchanged");
}
}

int main() {
  Generators::GroundMaterialCatalog materials;
  Generators::RegionSurfaceRecipes recipes;
  CHECK(materials.Load("data/world/ground-materials.json") &&
            recipes.Load("data/world/vegetation.json", materials),
        "the actual material and surface recipes load");
  if (!recipes.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  CHECK(field.Accept(Wire::kTileX, Wire::kTileX, Wire::Tile()).has_value(),
        "the declared MVT source loads");
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  const auto graph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  CHECK(graph.Graph != nullptr, "the native source graph builds");
  if (!graph.Graph) { return Report(); }
  constexpr std::array<GeographicBounds, 1> coverage{
      {{.WestDeg = 0, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 0}}};
  const auto made = BuildMvtTransportSnapshot(
      field,
      streets,
      *graph.Graph,
      coverage,
      {},
      {.NodeGroupPayloadBytesMost = kBudget, .SourceFactsPayloadBytesMost = kBudget});
  CHECK(made && made->SourceFacts && made->NodeGroups && made->NodeGroups->Groups.size() > 2,
        "the unchanged producer captures a nontrivial canonical partition");
  if (made && made->SourceFacts && made->NodeGroups && made->NodeGroups->Groups.size() > 2) {
    CheckPartitions(field, streets, *graph.Graph, *made);
  }
  return Report();
}
