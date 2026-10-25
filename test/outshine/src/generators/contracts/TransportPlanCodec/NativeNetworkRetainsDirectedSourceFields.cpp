#include "TransportPlanFixture.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::World;
using namespace outshine::Test;

std::shared_ptr<const TransportNetworkSnapshot>
WithDirectedFields(const TransportNetworkSnapshot &source) {
  const auto &graph = source.Topology();
  std::vector<TransportNode> nodes(graph.Nodes().begin(), graph.Nodes().end());
  std::vector<TransportEdge> edges(graph.Edges().begin(), graph.Edges().end());
  auto &edge = edges[0];
  edge.Modes = static_cast<uint8_t>(TransportMode::Rail);
  edge.Facility = TransportFacility::Railway;
  edge.Surface = TransportSurface::Rail;
  edge.WidthM = 7;
  edge.LaneCount = 1;
  edge.Layer = -2;
  edge.Bridge = true;
  edge.Tunnel = true;
  edge.Access = TransportAccess::Restricted;
  edges[1].Access = TransportAccess::Forbidden;
  return std::make_shared<const TransportNetworkSnapshot>(
      ResolvedTransport{.Graph = TransportTopology(graph.SourceIdentity(),
                                                   std::move(nodes),
                                                   std::move(edges),
                                                   graph.UnclassifiedPathCount()),
                        .Routes = {source.Routes().begin(), source.Routes().end()}},
      std::vector<GeographicBounds>{source.Coverage().begin(), source.Coverage().end()},
      source.Metrics());
}

void CheckNetwork() {
  auto plan = TransportPlanFixture();
  CHECK(plan && plan->Network, "the plan owns the existing native snapshot type");
  if (!plan || !plan->Network) { return; }
  plan->Network = WithDirectedFields(*plan->Network);
  const auto bytes = EncodeTransportPlan(*plan, kTransportFixtureLimits);
  CHECK(bytes.has_value(), "directed functional fields have a bounded native representation");
  if (!bytes) { return; }
  const auto restored = DecodeTransportPlan(*bytes, kTransportFixtureLimits);
  CHECK(restored && restored->Network, "the existing snapshot is restored with the plan");
  if (!restored || !restored->Network) { return; }
  const auto &network = *restored->Network;
  const auto &graph = network.Topology();
  const auto &edge = graph.Edges()[0];
  CHECK(network.SourceIdentity() == plan->Network->SourceIdentity() &&
            graph.UnclassifiedPathCount() == plan->Network->Topology().UnclassifiedPathCount() &&
            graph.Nodes().size() == 3 && graph.Edges().size() == 4 &&
            graph.Nodes()[0].SourceNodeId == 7 &&
            graph.Nodes()[0].LatitudeDeg == plan->Anchor.LatitudeDeg &&
            graph.Nodes()[1].LongitudeDeg == plan->Ports[0].Anchor.LongitudeDeg,
        "source identity, native coordinates and unclassified sources survive the asset boundary");
  CHECK(edge.Id.PathId == 42 && edge.Id.Direction == EdgeDirection::Forward &&
            edge.FromNodeId == 7 && edge.ToNodeId == 8 && edge.Allows(TransportMode::Rail) &&
            !edge.Allows(TransportMode::Motor) && edge.Facility == TransportFacility::Railway &&
            edge.Surface == TransportSurface::Rail && edge.WidthM == 7 && edge.LaneCount == 1 &&
            edge.Layer == -2 && edge.Bridge && edge.Tunnel &&
            edge.Access == TransportAccess::Restricted &&
            graph.Edges()[1].Id.Direction == EdgeDirection::Reverse &&
            graph.Edges()[1].Access == TransportAccess::Forbidden &&
            !graph.Edges()[1].Allows(TransportMode::Motor),
        "modes, access, facility, surface, width, lanes, layer and both flags stay directional");
  constexpr uint64_t sourceRoute = 84;
  const auto *route = network.FindRoute("fixture-circuit");
  CHECK(route && route->SourceIdentity == network.SourceIdentity() &&
            route->SourceRouteId == sourceRoute && route->StartNodeId == 7 &&
            route->EdgeIds.size() == 2 && route->EdgeIds[1].Direction == EdgeDirection::Reverse &&
            network.Coverage().size() == 1 &&
            network.Coverage()[0].EastDeg == plan->Network->Coverage()[0].EastDeg &&
            graph.OutgoingFrom(7).size() == 2 && graph.OutgoingFrom(7)[1].EdgeIndex == 3,
        "named routes, source-bound coverage and the prepared adjacency remain usable directly");
  CHECK(plan->Network->Metrics().SourceBytes != 0 && network.Metrics().SourceBytes == 0 &&
            network.Metrics().ReadMs == 0 && network.Metrics().ParseMs == 0 &&
            network.Metrics().GraphMs == 0,
        "past source IO and graph-construction times are transient instead of forged cache work");
  CHECK(EncodeTransportPlan(*restored, kTransportFixtureLimits) == bytes,
        "all persisted network and geometry fields reencode exactly");
  constexpr uint8_t sourcePath = 42;
  std::array<uint8_t, 32> edgePattern{};
  edgePattern[0] = sourcePath;
  edgePattern[16] = 7;
  edgePattern[3 * sizeof(uint64_t)] = 8;
  auto corrupt = *bytes;
  const auto match = std::ranges::search(corrupt, edgePattern);
  CHECK(!match.empty() && match.end() != corrupt.end(),
        "the independently specified native edge record is present in the wire product");
  if (!match.empty() && match.end() != corrupt.end()) {
    *match.end() = uint8_t{128};
    CHECK(!DecodeTransportPlan(corrupt, kTransportFixtureLimits),
          "unknown native mode bits cannot become a functional Ready snapshot");
  }
}

void CheckGuarantee() {
  auto plan = TransportPlanFixture();
  if (!plan) { return; }
  const auto native = plan->Network;
  const auto withMetadata = [&](std::vector<GeographicBounds> coverage,
                                std::vector<NamedCircuitRoute> routes) {
    return std::make_shared<const TransportNetworkSnapshot>(
        ResolvedTransport{.Graph = native->Topology(), .Routes = std::move(routes)},
        std::move(coverage),
        TransportLoadMetrics{});
  };
  std::vector<NamedCircuitRoute> routes(native->Routes().begin(), native->Routes().end());
  std::vector<GeographicBounds> coverage(native->Coverage().begin(), native->Coverage().end());
  plan->Network = withMetadata({}, routes);
  CHECK(!EncodeTransportPlan(*plan, kTransportFixtureLimits),
        "a complete scoped plan cannot omit source coverage");
  coverage[0].NorthDeg = std::numeric_limits<double>::quiet_NaN();
  plan->Network = withMetadata(coverage, routes);
  const auto badCoverage = ValidateTransportNetwork(*plan->Network);
  CHECK(!badCoverage && badCoverage.error() == TransportNetworkError::InvalidCoverage &&
            !EncodeTransportPlan(*plan, kTransportFixtureLimits),
        "invalid source coverage cannot cross the functional asset boundary");
  coverage.assign(native->Coverage().begin(), native->Coverage().end());
  routes[0].Circuit.EdgeIds[1].Direction = EdgeDirection::Forward;
  plan->Network = withMetadata(coverage, routes);
  const auto badRoute = ValidateTransportNetwork(*plan->Network);
  CHECK(!badRoute && badRoute.error() == TransportNetworkError::InvalidRoutes &&
            !EncodeTransportPlan(*plan, kTransportFixtureLimits),
        "a nonconnecting source circuit cannot be silently repaired on publication");
  plan->Network = std::make_shared<const TransportNetworkSnapshot>(
      ResolvedTransport{}, coverage, TransportLoadMetrics{});
  CHECK(!EncodeTransportPlan(*plan, kTransportFixtureLimits),
        "an unbound default native graph is not a complete source-backed product");
  plan->Network.reset();
  const auto absent = EncodeTransportPlan(*plan, kTransportFixtureLimits);
  CHECK(!absent && absent.error() == TransportPlanCodecError::InvalidNetwork,
        "a geometric-only complete plan cannot satisfy the functional v2 contract");
  plan->Coverage = TransportCoverage::Partial;
  plan->Omissions.push_back(
      {.SourceId = "fixture/road/42", .Reason = TransportOmissionReason::ResourceDeferred});
  const auto partial = EncodeTransportPlan(*plan, kTransportFixtureLimits);
  CHECK(partial.has_value(), "explicit incomplete geometry may still be stored as partial data");
  if (partial) {
    const auto restored = DecodeTransportPlan(*partial, kTransportFixtureLimits);
    CHECK(restored && !restored->Network && restored->Coverage == TransportCoverage::Partial,
          "decoding never upgrades an omitted functional network to complete coverage");
  }
}
}

int main() {
  CheckNetwork();
  CheckGuarantee();
  return Report();
}
