#include "TransportPlan.h"
#include "TransportPlanCodec.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

static_assert(!std::is_default_constructible_v<outshine::Generators::TransportInputScopeId>);
static_assert(!std::is_default_constructible_v<outshine::Generators::TransportPlanAddress>);
static_assert(std::is_const_v<outshine::Generators::TransportPlanLease::element_type>);

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::array<uint8_t, 32> binding{};
  for (size_t at = 0; at < binding.size(); ++at) { binding[at] = static_cast<uint8_t>(at); }
  const TransportPlanAddress address{.Definition = {.Core = {.Level = 9, .X = 270, .Y = 99}},
                                     .Inputs = TransportInputScopeId(binding)};
  const auto original = TransportPlanRequestKey(address);
  CHECK(original && *original == "5a6c58fe35f4864e2b19a0537693b432c2d854300fb315720e30ac0aa6d8b615",
        "the request key has independently encoded fixed fields and explicit domain version");
  CHECK(original && *original != "cf923a02cf23279bf813a229636646271ea653405a3097b781307836f9dace42",
        "source-end products cannot reuse the previous recipe's persistent request alias");
  CHECK(TransportPlanRequestKey(address) == original,
        "identical known metadata resolves without source or plan state");
  auto subset = address;
  subset.Definition.Representation = TransportRepresentation::FunctionalSubset;
  subset.Definition.RepresentationVersion = 1;
  const auto subsetKey = TransportPlanRequestKey(subset);
  CHECK(subsetKey &&
            *subsetKey == "49a7a887ed2419ccf69eec039ddf7fb77e342e8be04fcc8c6aef14c43582ff64",
        "the subset key matches independently encoded fields and its separate versioned identity");
  CHECK(TransportPlanContentKey(subset.Definition, binding) !=
            TransportPlanContentKey(address.Definition, binding),
        "identical resolved inputs do not alias full and subset content contracts");
  subset.Definition.RepresentationVersion = 2;
  CHECK(!TransportPlanRequestKey(subset),
        "unknown subset guarantees cannot fall back to a complete request");
  auto moved = address;
  ++moved.Definition.Core.X;
  CHECK(TransportPlanRequestKey(moved) != original,
        "a different geographic core has an independent logical lookup");
  auto rebound = address;
  rebound.Inputs.Bytes[0] ^= uint8_t{1};
  CHECK(TransportPlanRequestKey(rebound) != original,
        "an explicitly changed source binding cannot reuse the old request");
  auto unsupported = address;
  ++unsupported.Definition.RepresentationVersion;
  const auto version = TransportPlanRequestKey(unsupported);
  CHECK(!version && version.error() == TransportPlanRequestError::UnsupportedRepresentation,
        "an unsupported functional contract is rejected before lookup or input work");
  unsupported = address;
  unsupported.Definition.RepresentationVersion = 1;
  CHECK(!TransportPlanRequestKey(unsupported),
        "the old geometric-only representation cannot alias a functional network lookup");
  unsupported = address;
  unsupported.Definition.Representation =
      static_cast<TransportRepresentation>(std::numeric_limits<uint8_t>::max());
  CHECK(!TransportPlanRequestKey(unsupported), "unknown representation IDs are not aliases");
  auto invalid = address;
  invalid.Definition.Core.Y = uint32_t{1} << static_cast<unsigned>(invalid.Definition.Core.Level);
  const auto core = TransportPlanRequestKey(invalid);
  CHECK(!core && core.error() == TransportPlanRequestError::InvalidCore,
        "an out-of-domain core does not acquire a persistent identity");
  const TransportPlanAddress zero{.Definition = {},
                                  .Inputs = TransportInputScopeId(std::array<uint8_t, 32>{})};
  CHECK(TransportPlanRequestKey(zero).has_value(),
        "an explicitly supplied zero digest is not confused with absent source binding");
  const TransportPlan plan(address.Definition);
  CHECK(plan.Coverage == TransportCoverage::Unknown,
        "constructing the data contract does not claim complete functional coverage");
  return Report();
}
