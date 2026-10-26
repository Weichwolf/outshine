#ifndef OUTSHINE_TEST_TRANSPORTPLANSOURCEFIXTURE_H
#define OUTSHINE_TEST_TRANSPORTPLANSOURCEFIXTURE_H
#include "TransportPlanFixture.h"
#include "TransportProvenanceFixture.h"
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace outshine::Test {
inline std::shared_ptr<const World::TransportNetworkSnapshot>
WithTransportProvenance(const World::TransportNetworkSnapshot &source,
                        World::TransportProvenance provenance) {
  return std::make_shared<const World::TransportNetworkSnapshot>(
      World::ResolvedTransport{.Graph = source.Topology(),
                               .Routes = {source.Routes().begin(), source.Routes().end()}},
      std::vector<GeographicBounds>{source.Coverage().begin(), source.Coverage().end()},
      source.Metrics(),
      std::move(provenance));
}

inline std::optional<Generators::TransportPlan> TransportPlanSourceFixture() {
  auto plan = TransportPlanFixture();
  if (!plan || !plan->Network) { return std::nullopt; }
  auto provenance = TransportProvenanceFixture();
  plan->Inputs.clear();
  for (const auto &source : provenance.Sources) {
    plan->Inputs.push_back({.DatasetId = source.Identity.DatasetId,
                            .Version = "v1",
                            .Revision = source.Identity.Revision,
                            .ItemId = source.Item,
                            .Content = source.Content});
  }
  const auto binding =
      Generators::TransportPlanInputBinding(plan->Inputs, kTransportFixtureLimits.PackageBytesMost);
  if (!binding) { return std::nullopt; }
  plan->InputBinding = *binding;
  plan->Network = WithTransportProvenance(*plan->Network, std::move(provenance));
  return plan;
}
}
#endif
