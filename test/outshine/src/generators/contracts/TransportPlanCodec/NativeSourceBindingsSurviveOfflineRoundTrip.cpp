#include "TransportPlanSourceFixture.h"
#include "TransportPlanCodec.h"
#include "TransportProvenance.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::World;

void CheckMalformedReference(const std::vector<uint8_t> &encoded) {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr size_t fragmentAt = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint64_t);
  constexpr size_t ordinalAt = fragmentAt + 32;
  std::array<uint8_t, ordinalAt + sizeof(uint32_t)> reference{};
  reference[sizeof(uint32_t)] = 1;
  reference[fragmentAt] = 3;
  reference[ordinalAt] = 3;
  auto corrupt = encoded;
  const auto found = std::ranges::search(corrupt, reference);
  CHECK(!found.empty(), "the independently specified reference record is present");
  if (found.empty()) { return; }
  *found.begin() = 2;
  CHECK(!DecodeTransportPlan(corrupt, kTransportFixtureLimits),
        "a forged stored source index cannot become an offline functional snapshot");
}
}

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  auto plan = TransportPlanSourceFixture();
  CHECK(plan && plan->Network, "a source-bound native plan is available");
  if (!plan || !plan->Network) { return Report(); }
  auto barePlan = *plan;
  barePlan.Network = WithTransportProvenance(*barePlan.Network, {});
  const auto bare = EncodeTransportPlan(barePlan, kTransportFixtureLimits);
  CHECK(bare.has_value(), "native source-element products remain serializable");
  if (!bare) { return Report(); }
  const auto original = DecodeTransportPlan(*bare, kTransportFixtureLimits);
  CHECK(original && original->Network &&
            original->Network->Provenance().IdentitySpace ==
                TransportIdentitySpace::SourceElements &&
            original->Network->Provenance().Sources.empty(),
        "original element identities remain distinct from derived fragments");
  const size_t originalCapacity = TransportPlanCapacityBytes(barePlan);
  const auto encoded = EncodeTransportPlan(*plan, kTransportFixtureLimits);
  CHECK(encoded.has_value(), "the plan persists complete native provenance within its budget");
  if (!encoded) { return Report(); }
  const auto decoded = DecodeTransportPlan(*encoded, kTransportFixtureLimits);
  CHECK(decoded && decoded->Network, "offline restoration requires no source or topology rebuild");
  if (!decoded || !decoded->Network) { return Report(); }
  const auto &provenance = decoded->Network->Provenance();
  CHECK(provenance.IdentitySpace == TransportIdentitySpace::DerivedFragments &&
            provenance.Sources.size() == 2 && provenance.Paths.size() == 1 &&
            provenance.References.size() == 2,
        "the model, deduplicated source table and multiple-source span survive");
  if (provenance.Sources.size() != 2 || provenance.Paths.size() != 1 ||
      provenance.References.size() != 2) {
    return Report();
  }
  CHECK(provenance.Sources[0].Identity.DatasetId == "source-a" &&
            provenance.Sources[0].Identity.Revision == "revision-1" &&
            provenance.Sources[0].Item == "opaque-item-a" &&
            provenance.Sources[0].Content[0] == 1 && provenance.Sources[1].Content[0] == 2 &&
            provenance.References[0].ProviderElementId == 0 &&
            !provenance.References[1].ProviderElementId &&
            provenance.References[0].Fragment[0] == 3 &&
            provenance.References[1].Fragment[0] == 4 &&
            provenance.References[0].SegmentOrdinal == 3 &&
            provenance.References[1].SegmentOrdinal == 7,
        "actual identity/digest, optional zero ID, fragment and source-segment ordinals survive");
  CHECK(provenance.Paths[0].PathId == 42 && provenance.Paths[0].FirstReference == 0 &&
            provenance.Paths[0].ReferenceCount == 2 &&
            provenance.Paths[0].SuppliedFields ==
                (static_cast<uint16_t>(TransportAttribute::Width) |
                 static_cast<uint16_t>(TransportAttribute::Direction)) &&
            provenance.Paths[0].DerivedFields == static_cast<uint16_t>(TransportAttribute::Width),
        "field masks preserve supplied, derived, overlapping and unknown states");
  CHECK(EncodeTransportPlan(*decoded, kTransportFixtureLimits) == encoded,
        "the restored plan reencodes byte-identically without normalizing sources");
  CheckMalformedReference(*encoded);
  auto invalid = provenance;
  invalid.References[0].Source = 2;
  plan->Network = WithTransportProvenance(*plan->Network, std::move(invalid));
  CHECK(!EncodeTransportPlan(*plan, kTransportFixtureLimits),
        "invalid source references cannot become a stored functional product");
  invalid = provenance;
  invalid.Sources[0].Content[0] = 2;
  plan->Network = WithTransportProvenance(*plan->Network, std::move(invalid));
  CHECK(!EncodeTransportPlan(*plan, kTransportFixtureLimits),
        "source rows must belong to the actual bound input manifest");
  auto oldFormat = *encoded;
  oldFormat[5] = 2;
  CHECK(!DecodeTransportPlan(oldFormat, kTransportFixtureLimits),
        "a prior source-free plan format is a local miss rather than a provenance-complete hit");
  std::println("TRANSPORT_PROVENANCE package_extra_bytes={} capacity_extra_bytes={}",
               encoded->size() - bare->size(),
               TransportPlanCapacityBytes(*decoded) - originalCapacity);
  return Report();
}
