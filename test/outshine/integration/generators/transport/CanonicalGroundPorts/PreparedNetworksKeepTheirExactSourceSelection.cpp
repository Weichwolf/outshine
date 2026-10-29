#include "CanonicalGroundFieldFixture.h"
#include "GroundPortPlan.h"
#include "TransportPlanNetworkBinding.h"
#include "TransportNetworkSnapshot.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <memory>
#include <stop_token>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void CheckBounds(const GroundPortField &field, const GroundPortPlanNetwork &prepared) {
  const auto failedAs = [](const auto &result, auto error) {
    const auto *actual = result ? nullptr : std::get_if<decltype(error)>(&result.error());
    return actual != nullptr && *actual == error;
  };
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = GroundPortPlanNetwork::Prepare(field, {}, 1048576, stop.get_token());
  CHECK(failedAs(canceled, TransportPlanNetworkError::Canceled),
        "canceled preparation cannot publish reusable field inputs");
  const auto edges = GroundPortPlanNetwork::Prepare(field, {.DirectedEdges = 1});
  CHECK(failedAs(edges, TransportPlanNetworkError::CapacityExceeded),
        "source projection still admits both actual directed aliases before allocation");
  const auto tiny = GroundPortPlanNetwork::Prepare(field, {}, 0);
  CHECK(failedAs(tiny, TransportPlanCodecError::CapacityExceeded),
        "network content hashing honors its independent byte bound");
  const auto tooSmall =
      GroundPortPlanNetwork::Prepare(field, {.OutputCapacityBytes = prepared.CapacityBytes() - 1});
  CHECK(failedAs(tooSmall, TransportPlanNetworkError::CapacityExceeded),
        "prepared metadata and owned native network share the declared capacity bound");
  const auto exact =
      GroundPortPlanNetwork::Prepare(field, {.OutputCapacityBytes = prepared.CapacityBytes()});
  CHECK(exact && exact->CapacityBytes() == prepared.CapacityBytes() &&
            exact->Binding() == prepared.Binding(),
        "the exact admitted capacity preserves the complete field network content");
}

void CheckForeign(const std::vector<GroundPortField> &fields,
                  const std::vector<GroundPortPlanNetwork> &prepared) {
  CHECK(fields[0].Source().Source == fields[1].Source().Source &&
            fields[0].Source().SourceEdges != fields[1].Source().SourceEdges,
        "the real five/six-span controls share a source owner but select different native edges");
  CHECK(prepared[0].Binding() != prepared[1].Binding() &&
            prepared[2].Binding() == prepared[3].Binding(),
        "selected content changes the digest while equal republished content preserves identity");
  for (size_t at = 0; at < fields.size(); ++at) {
    const auto binding = TransportPlanNetworkBinding(prepared[at].Projection().Network, 1048576);
    CHECK(binding && *binding == prepared[at].Binding(),
          "the retained digest binds every owned native network field");
    CHECK(prepared[at].Matches(fields[at]),
          "the prepared product accepts its exact immutable native source selection");
    const size_t other = (at + 1) % fields.size();
    CHECK(!prepared[at].Matches(fields[other]),
          "another field or republished native source cannot silently reuse an input owner");
  }
  const auto body = BuildGroundPortBodies(fields[0], {.ThicknessM = .3, .ApronM = 2, .ReliefM = 3});
  CHECK(body.has_value(), "the unchanged whole-field body remains a valid source-bound input");
  if (!body) { return; }
  const std::array omissions{TransportOmission{.SourceId = "fixture/unresolved",
                                               .Reason = TransportOmissionReason::MissingBoundary}};
  const auto refused = BuildGroundPortPartialPlan(fields[0], prepared[1], *body, {}, {}, omissions);
  CHECK(!refused && std::holds_alternative<TransportPlanNetworkError>(refused.error()) &&
            std::get<TransportPlanNetworkError>(refused.error()) ==
                TransportPlanNetworkError::InvalidSource,
        "a foreign prepared network fails before any core selection or body work");
}
}

int main() {
  auto fields = CanonicalGroundFixture::LoadFields();
  CHECK(fields.size() == 4, "all actual field variants and both source publications load");
  if (fields.size() != 4) { return Report(); }
  std::vector<GroundPortPlanNetwork> prepared;
  prepared.reserve(fields.size());
  for (const auto &field : fields) {
    auto network = GroundPortPlanNetwork::Prepare(field);
    CHECK(network.has_value(), "the native field projection and binding prepare once");
    if (!network) { return Report(); }
    prepared.push_back(std::move(*network));
  }
  CheckBounds(fields[0], prepared[0]);
  CheckForeign(fields, prepared);
  const std::weak_ptr<const World::TransportNetworkSnapshot> original = fields[2].Source().Source;
  fields.clear();
  CHECK(original.expired(),
        "prepared inputs retain only a weak original identity, not the full source lease");
  CHECK(World::ValidateTransportNetwork(*prepared[2].Projection().Network),
        "the owned projected native graph survives release of all original fields");
  return Report();
}
