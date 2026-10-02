#include "TransportNetworkSnapshot.h"
#include "Check.h"

#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::World;
  using namespace outshine::Test;
  const TransportEdgeId first{.PathId = 8, .SegmentOrdinal = 0};
  const TransportEdgeId second{.PathId = 8, .SegmentOrdinal = 1};
  const TransportEdgeId closing{.PathId = 8, .SegmentOrdinal = 2};
  const TransportEdgeId blocked{.PathId = 9, .SegmentOrdinal = 0};
  std::vector<TransportNode> nodes{{.SourceNodeId = 3}, {.SourceNodeId = 1}, {.SourceNodeId = 2}};
  std::vector<TransportEdge> edges{{.Id = closing,
                                    .FromNodeId = 3,
                                    .ToNodeId = 1,
                                    .Modes = static_cast<uint8_t>(TransportMode::Motor)},
                                   {.Id = blocked,
                                    .FromNodeId = 1,
                                    .ToNodeId = 3,
                                    .Modes = static_cast<uint8_t>(TransportMode::Walk),
                                    .Access = TransportAccess::Forbidden},
                                   {.Id = second,
                                    .FromNodeId = 2,
                                    .ToNodeId = 3,
                                    .Modes = static_cast<uint8_t>(TransportMode::Motor)},
                                   {.Id = first,
                                    .FromNodeId = 1,
                                    .ToNodeId = 2,
                                    .Modes = static_cast<uint8_t>(TransportMode::Motor)}};
  TransportTopology graph(
      {.DatasetId = "external-native", .Revision = "r1"}, std::move(nodes), std::move(edges), 1);
  CHECK(graph.Nodes().size() == 3 && graph.Nodes()[0].SourceNodeId == 1 &&
            graph.Edges()[0].Id == first && graph.Edges()[1].Id == second,
        "native inputs are indexed deterministically without a source parser");
  const auto outgoing = graph.OutgoingFrom(1);
  CHECK(outgoing.size() == 2 && graph.Edges()[outgoing[0].EdgeIndex].Id == first &&
            graph.Edges()[outgoing[1].EdgeIndex].Id == blocked && graph.OutgoingFrom(99).empty(),
        "native adjacency selects exactly the independent expected edges");
  CHECK(graph.FindNode(2) && !graph.FindNode(99) && graph.FindEdge(first) &&
            !graph.FindEdge({.PathId = 99}) && graph.UnclassifiedPathCount() == 1,
        "native lookup and unknown-path accounting need no OSM knowledge");
  CHECK(TransportTopology::CanContinue(*graph.FindEdge(first), *graph.FindEdge(second)) &&
            !TransportTopology::CanContinue(*graph.FindEdge(first), *graph.FindEdge(blocked)),
        "connectivity and access remain native graph semantics");
  const Data::SourceIdentity identity = graph.SourceIdentity();
  ResolvedTransport transport{.Graph = std::move(graph),
                              .Routes = {{.Id = "native-loop",
                                          .Circuit = {.SourceIdentity = identity,
                                                      .SourceRouteId = 100,
                                                      .StartNodeId = 1,
                                                      .EdgeIds = {first, second, closing}}}}};
  TransportNetworkSnapshot published(std::move(transport), {}, {.SourceBytes = 12});
  CHECK(
      published.SourceIdentity() == identity && published.FindRoute("native-loop") &&
          !published.FindRoute("missing") && published.RouteEdgeCount() == 3 &&
          published.Topology().FindNode(3) && published.Metrics().SourceBytes == 12,
      "closed native publication retains topology and provenance independently of source formats");
  return Report();
}
