#include "TransportNetworkFixture.h"
#include "TransportPlanFixture.h"
#include "TransportProvenanceFixture.h"
#include "Check.h"
#include "TransportPlanNetwork.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
using namespace outshine::World;

std::shared_ptr<const TransportNetworkSnapshot> Source() {
  const auto base =
      TransportNetworkFixture({}, {.WestDeg = -1, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1});
  std::vector<TransportNode> nodes(base->Topology().Nodes().begin(),
                                   base->Topology().Nodes().end());
  std::vector<TransportEdge> edges(base->Topology().Edges().begin(),
                                   base->Topology().Edges().end());
  auto edge = edges.front();
  edge.Id = {.PathId = 43, .SegmentOrdinal = 8};
  edge.FromNodeId = 8;
  edge.ToNodeId = 9;
  edge.Modes =
      static_cast<uint8_t>(TransportMode::Rail) | static_cast<uint8_t>(TransportMode::Walk);
  edge.Facility = TransportFacility::Mixed;
  edge.Surface = TransportSurface::Rail;
  edge.WidthM = 5.25;
  edge.LaneCount = 1;
  edge.Layer = -2;
  edge.Tunnel = true;
  edge.Access = TransportAccess::Restricted;
  edges.push_back(edge);
  edge.Id.Direction = EdgeDirection::Reverse;
  std::swap(edge.FromNodeId, edge.ToNodeId);
  edges.push_back(edge);
  auto provenance = TransportProvenanceFixture();
  provenance.Paths.push_back({.PathId = 43,
                              .FirstReference = 2,
                              .ReferenceCount = 1,
                              .SuppliedFields = 1023,
                              .DerivedFields = 16});
  provenance.References.push_back({.Source = 1, .SegmentOrdinal = 19, .ProviderElementId = 1234});
  provenance.References.back().Fragment.fill(19);
  std::vector<NamedCircuitRoute> routes(base->Routes().begin(), base->Routes().end());
  routes.push_back({.Id = "second-circuit",
                    .Circuit = {.SourceIdentity = base->SourceIdentity(),
                                .SourceRouteId = 85,
                                .StartNodeId = 8,
                                .EdgeIds = {{.PathId = 43, .SegmentOrdinal = 8}, edge.Id}}});
  return std::make_shared<const TransportNetworkSnapshot>(
      ResolvedTransport{
          .Graph = TransportTopology(base->SourceIdentity(), std::move(nodes), std::move(edges), 3),
          .Routes = std::move(routes)},
      std::vector<GeographicBounds>{{.WestDeg = -1, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1}},
      TransportLoadMetrics{},
      std::move(provenance));
}

void CheckRows(const TransportNetworkSnapshot &source) {
  const std::array ids{
      TransportEdgeId{.PathId = 43, .SegmentOrdinal = 8, .Direction = EdgeDirection::Reverse},
      TransportEdgeId{.PathId = 43, .SegmentOrdinal = 8},
      TransportEdgeId{.PathId = 43, .SegmentOrdinal = 8}};
  const auto projected = ProjectTransportPlanNetwork(source, ids);
  CHECK(projected.has_value(), "exact directed native rows form a bounded network projection");
  if (!projected) { return; }
  const auto &network = *projected->Network;
  CHECK(ValidateTransportNetwork(network).has_value() && network.Coverage().empty() &&
            network.SourceIdentity() == source.SourceIdentity(),
        "the network preserves source identity while making no geographic coverage claim");
  CHECK(network.Topology().Edges().size() == 2 && network.Topology().Nodes().size() == 2 &&
            network.Topology().FindEdge({.PathId = 42}) == nullptr &&
            network.Topology().UnclassifiedPathCount() == 3,
        "unselected rows are absent and the inherited source diagnostic is preserved");
  for (const auto &edge : network.Topology().Edges()) {
    const auto *original = source.Topology().FindEdge(edge.Id);
    CHECK(original && edge.FromNodeId == original->FromNodeId &&
              edge.ToNodeId == original->ToNodeId && edge.Modes == original->Modes &&
              edge.Facility == original->Facility && edge.Surface == original->Surface &&
              edge.WidthM == original->WidthM && edge.LaneCount == original->LaneCount &&
              edge.Layer == original->Layer && edge.Bridge == original->Bridge &&
              edge.Tunnel == original->Tunnel && edge.Access == original->Access,
          "modes, access, surface, profile attributes and directed endpoints remain exact");
  }
  const auto &provenance = network.Provenance();
  CHECK(provenance.Sources.size() == 1 && provenance.Sources[0].Item == "opaque-item-b" &&
            provenance.Sources[0].Content == source.Provenance().Sources[1].Content &&
            provenance.Paths.size() == 1 && provenance.Paths[0].FirstReference == 0 &&
            provenance.Paths[0].SuppliedFields == 1023 && provenance.Paths[0].DerivedFields == 16 &&
            provenance.References.size() == 1 && provenance.References[0].Source == 0 &&
            provenance.References[0].SegmentOrdinal == 19 &&
            provenance.References[0].ProviderElementId == 1234 &&
            provenance.References[0].Fragment == source.Provenance().References[2].Fragment,
        "only source table indices change; original segment, provider and fragment identities do "
        "not");
  CHECK(network.RouteCount() == 1 && network.FindRoute("second-circuit") != nullptr &&
            projected->OmittedRoutes.size() == 1 &&
            projected->OmittedRoutes[0].Id == "fixture-circuit" &&
            projected->OmittedRoutes[0].SourceRouteId == 84,
        "only complete original circuits survive and omitted route identities are explicit");
  const std::array oneWay{ids[1]};
  const auto directed = ProjectTransportPlanNetwork(source, oneWay);
  CHECK(directed && directed->Network->Topology().Edges().size() == 1 &&
            directed->Network->RouteCount() == 0 && directed->OmittedRoutes.size() == 2 &&
            directed->Network->Topology().OutgoingFrom(9).empty(),
        "the projection never invents an opposite direction or closes a truncated circuit");
}

void CheckCodec(const TransportNetworkSnapshot &source) {
  auto plan = TransportPlanFixture();
  CHECK(plan.has_value(), "the existing functional plan fixture remains available");
  if (!plan) { return; }
  const std::array ids{TransportEdgeId{.PathId = 42},
                       TransportEdgeId{.PathId = 42, .Direction = EdgeDirection::Reverse}};
  const auto projected = ProjectTransportPlanNetwork(source, ids);
  CHECK(projected.has_value(), "the first physical part retains both original directed aliases");
  if (!projected) { return; }
  plan->Network = projected->Network;
  plan->Coverage = TransportCoverage::Partial;
  plan->Omissions = {{.SourceId = "fixture/road/42/1"}};
  plan->Paths.resize(1);
  plan->Stations.resize(2);
  for (const auto &item : plan->Network->Provenance().Sources) {
    plan->Inputs.push_back({.DatasetId = item.Identity.DatasetId,
                            .Version = "original-format",
                            .Revision = item.Identity.Revision,
                            .ItemId = item.Item,
                            .Content = item.Content});
  }
  const auto binding =
      TransportPlanInputBinding(plan->Inputs, kTransportFixtureLimits.PackageBytesMost);
  CHECK(binding.has_value(), "all consumed original references bind the actual plan input table");
  if (!binding) { return; }
  plan->InputBinding = *binding;
  const auto bytes = EncodeTransportPlan(*plan, kTransportFixtureLimits);
  CHECK(bytes.has_value(),
        "the bounded network crosses the existing native plan serialization boundary");
  if (!bytes) { return; }
  const std::array reordered{ids[1], ids[0], ids[1]};
  const auto repeated = ProjectTransportPlanNetwork(source, reordered, {.DirectedEdges = 64});
  CHECK(repeated.has_value(), "duplicate requests and different admission caps remain valid");
  if (repeated) {
    plan->Network = repeated->Network;
    const auto same = EncodeTransportPlan(*plan, kTransportFixtureLimits);
    CHECK(
        same && *same == *bytes,
        "request order, duplicate IDs and capacity choices never change serialized native content");
  }
  const auto restored = DecodeTransportPlan(*bytes, kTransportFixtureLimits);
  CHECK(restored && restored->Coverage == TransportCoverage::Partial && restored->Network &&
            restored->Network->Topology().Edges().size() == 2 &&
            restored->Network->Provenance().References.size() == 2 &&
            restored->Network->Provenance().References[0].SegmentOrdinal == 3 &&
            restored->Network->Provenance().References[1].SegmentOrdinal == 7 &&
            restored->Network->Coverage().empty(),
        "codec preserves the selected graph and full path references rather than filtering by "
        "graph ordinals");
  plan->Coverage = TransportCoverage::Complete;
  plan->Omissions.clear();
  const auto incomplete = EncodeTransportPlan(*plan, kTransportFixtureLimits);
  CHECK(!incomplete && incomplete.error() == TransportPlanCodecError::InvalidNetwork,
        "a bounded network without geographic closure cannot become a Complete plan");
  plan->Coverage = TransportCoverage::Partial;
  plan->Omissions = {{.SourceId = "fixture/road/42/1"}};
  plan->Inputs.back().Content[0] ^= 1;
  CHECK(!EncodeTransportPlan(*plan, kTransportFixtureLimits),
        "a projected network cannot cross the plan boundary with foreign input content");
}

void CheckLimits(const TransportNetworkSnapshot &source) {
  const std::array ids{TransportEdgeId{.PathId = 43, .SegmentOrdinal = 8}};
  for (unsigned which = 0; which < 6; ++which) {
    TransportPlanNetworkLimits limits;
    if (which == 0) { limits.DirectedEdges = 0; }
    if (which == 1) { limits.References = 0; }
    if (which == 2) { limits.Routes = 0; }
    if (which == 3) { limits.RouteEdges = 0; }
    if (which == 4) { limits.TextBytes = 0; }
    if (which == 5) { limits.OutputCapacityBytes = 0; }
    const auto result = ProjectTransportPlanNetwork(source, ids, limits);
    CHECK(!result && result.error() == TransportPlanNetworkError::CapacityExceeded,
          "each explicit work/storage limit refuses rather than publishing a truncated product");
  }
  std::stop_source canceled;
  canceled.request_stop();
  const auto stopped = ProjectTransportPlanNetwork(source, ids, {}, canceled.get_token());
  CHECK(!stopped && stopped.error() == TransportPlanNetworkError::Canceled,
        "cancellation cannot publish an apparently finished projection");
  const std::array missing{TransportEdgeId{.PathId = 999}};
  const auto absent = ProjectTransportPlanNetwork(source, missing);
  CHECK(!absent && absent.error() == TransportPlanNetworkError::MissingEdge,
        "unknown source IDs remain an explicit input failure");
  const auto empty = ProjectTransportPlanNetwork(source, {});
  CHECK(!empty && empty.error() == TransportPlanNetworkError::EmptySelection,
        "an empty request does not fabricate a network scope");
}

void CheckIndependentLifetime() {
  auto source = Source();
  const std::weak_ptr<const TransportNetworkSnapshot> old = source;
  const std::array ids{TransportEdgeId{.PathId = 43, .SegmentOrdinal = 8}};
  const auto result = ProjectTransportPlanNetwork(*source, ids);
  source.reset();
  CHECK(result && old.expired() && ValidateTransportNetwork(*result->Network).has_value(),
        "the projected product owns its native data without retaining the full source snapshot");
}

void CheckMalformedReferences(const TransportNetworkSnapshot &source) {
  const std::array ids{TransportEdgeId{.PathId = 43, .SegmentOrdinal = 8}};
  for (unsigned which = 0; which < 3; ++which) {
    auto provenance = source.Provenance();
    if (which == 0) { provenance.Sources.clear(); }
    if (which == 1) {
      provenance.Paths.back().FirstReference = std::numeric_limits<uint32_t>::max();
    }
    if (which == 2) { provenance.References.back().Source = std::numeric_limits<uint32_t>::max(); }
    const TransportNetworkSnapshot broken(
        {.Graph = source.Topology(), .Routes = {}}, {}, {}, std::move(provenance));
    const auto result = ProjectTransportPlanNetwork(broken, ids);
    CHECK(!result && result.error() == TransportPlanNetworkError::InvalidSource,
          "malformed path/source ranges fail explicitly before any invalid table access");
  }
}
}

int main() {
  const auto source = Source();
  CHECK(ValidateTransportNetwork(*source).has_value(), "the original attributed network is valid");
  CheckRows(*source);
  CheckCodec(*source);
  CheckLimits(*source);
  CheckIndependentLifetime();
  CheckMalformedReferences(*source);
  return Report();
}
