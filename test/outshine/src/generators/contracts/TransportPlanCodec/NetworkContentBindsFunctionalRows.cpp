#include "TransportNetworkFixture.h"
#include "TransportPlanNetworkBinding.h"
#include "Check.h"
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::World;

std::shared_ptr<const TransportNetworkSnapshot> Changed(const TransportNetworkSnapshot &source,
                                                        unsigned field) {
  auto nodes = std::vector<TransportNode>(source.Topology().Nodes().begin(),
                                          source.Topology().Nodes().end());
  auto edges = std::vector<TransportEdge>(source.Topology().Edges().begin(),
                                          source.Topology().Edges().end());
  if (field == 0) { edges[0].Access = TransportAccess::Forbidden; }
  if (field == 1) { nodes[0].LatitudeDeg += .001; }
  return std::make_shared<const TransportNetworkSnapshot>(
      ResolvedTransport{.Graph = TransportTopology(source.SourceIdentity(),
                                                   std::move(nodes),
                                                   std::move(edges),
                                                   source.Topology().UnclassifiedPathCount()),
                        .Routes = {source.Routes().begin(), source.Routes().end()}},
      std::vector<GeographicBounds>(source.Coverage().begin(), source.Coverage().end()),
      TransportLoadMetrics{.SourceBytes = 999, .ReadMs = 99, .ParseMs = 88, .GraphMs = 77},
      source.Provenance());
}
}

int main() {
  using namespace outshine::Test;
  const auto source =
      TransportNetworkFixture({}, {.WestDeg = -1, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1});
  constexpr size_t bytesMost = 1048576;
  const auto original = TransportPlanNetworkBinding(source, bytesMost);
  CHECK(original.has_value(), "the actual native network codec provides a content binding");
  for (const unsigned field : {0U, 1U, 2U}) {
    const auto copy = Changed(*source, field);
    const auto binding = TransportPlanNetworkBinding(copy, bytesMost);
    CHECK(binding.has_value(),
          "the independently owned changed graph remains a valid native input");
    if (!binding || !original) { continue; }
    CHECK((*binding == *original) == (field == 2),
          "functional node/attribute changes bind content while acquisition metrics do not");
  }
  const auto larger = TransportPlanNetworkBinding(source, bytesMost * 2);
  CHECK(original && larger && *original == *larger,
        "encoding capacity and container addresses are not native network identity");
  const auto absent = TransportPlanNetworkBinding({}, bytesMost);
  CHECK(!absent && absent.error() == TransportPlanCodecError::InvalidNetwork,
        "a missing network never receives a plausible native content digest");
  const auto limited = TransportPlanNetworkBinding(source, 0);
  CHECK(!limited && limited.error() == TransportPlanCodecError::CapacityExceeded,
        "bounded encoding failure is distinct from valid native content");
  return Report();
}
