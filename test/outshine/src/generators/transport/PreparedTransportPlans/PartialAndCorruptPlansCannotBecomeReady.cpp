#include "PreparedTransportPlans.h"
#include "TransportPlanFixture.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <system_error>
#include <unistd.h>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void Exercise(const std::string &directory) {
  auto store = PreparedTransportPlans::Open(directory);
  auto raw = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  auto plan = TransportPlanFixture();
  CHECK(store && raw && plan, "the store and independent fault injector are available");
  if (!store || !raw || !plan) { return; }
  const Box bounds{.Min = {{-1, -1, -1}}, .Max = {{1, 1, 1}}};
  const auto record = (*store)->Store(*plan, bounds, kTransportFixtureLimits);
  CHECK(record.has_value(), "the complete control is published");
  if (!record) { return; }
  const TransportPlanAddress address{.Definition = plan->Definition,
                                     .Inputs = TransportInputScopeId(std::array<uint8_t, 32>{})};
  auto geometryOnly = *plan;
  geometryOnly.Network.reset();
  const auto missingNetwork = (*store)->Store(geometryOnly, bounds, kTransportFixtureLimits);
  CHECK(!missingNetwork && missingNetwork.error() == TransportPlanAssetError::InvalidPlan,
        "a complete geometric-only product cannot enter the functional asset store");
  auto partial = *plan;
  partial.Coverage = TransportCoverage::Partial;
  partial.Omissions.push_back(
      {.SourceId = "fixture/unresolved", .Reason = TransportOmissionReason::ResourceDeferred});
  const auto refused = (*store)->Store(partial, bounds, kTransportFixtureLimits);
  CHECK(!refused && refused.error() == TransportPlanAssetError::InvalidPlan,
        "budget-limited partial data cannot replace a complete product under the same key");
  CHECK(!(*store)->BindRequest(
            address, {.Plan = std::make_shared<const TransportPlan>(partial), .Record = *record}),
        "a partial lease cannot bind a complete request even with a valid existing receipt");
  const auto control =
      (*store)->LoadProduct(plan->Definition, plan->InputBinding, kTransportFixtureLimits);
  CHECK(control && *control && (**control).Record.Package == record->Package,
        "rejected partial publication preserves the complete package");
  const TransportPlanCodecLimits tiny{.PackageBytesMost = kTransportFixtureLimits.PackageBytesMost,
                                      .DecodedCapacityMost = sizeof(TransportPlan) - 1};
  const auto capped = (*store)->LoadProduct(plan->Definition, plan->InputBinding, tiny);
  CHECK(!capped && capped.error() == TransportPlanAssetError::CapacityExceeded,
        "insufficient residency admission is distinct from a malformed product");
  std::stop_source cancel;
  cancel.request_stop();
  const auto canceled = (*store)->Store(*plan, bounds, kTransportFixtureLimits, cancel.get_token());
  CHECK(!canceled && canceled.error() == TransportPlanAssetError::Canceled,
        "a canceled cook cannot publish a result");
  auto invalid = EncodeTransportPlan(*plan, kTransportFixtureLimits);
  CHECK(invalid.has_value(), "the controlled corruption starts with an encoded plan");
  if (!invalid) { return; }
  (*invalid)[0] ^= uint8_t{1};
  auto injecting = *record;
  injecting.Package.clear();
  CHECK((*raw)->Publish(std::span(&injecting, size_t{1}), *invalid),
        "the injector stores an invalid plan with a valid package checksum");
  const auto corrupt =
      (*store)->LoadProduct(plan->Definition, plan->InputBinding, kTransportFixtureLimits);
  CHECK(corrupt && !*corrupt, "codec failure is a cache miss rather than a Ready lease");
  const auto partialBytes = EncodeTransportPlan(partial, kTransportFixtureLimits);
  auto partialRecord = *record;
  partialRecord.Package.clear();
  if (partialBytes) { partialRecord.ByteCount = partialBytes->size(); }
  CHECK(partialBytes && (*raw)->Publish(std::span(&partialRecord, size_t{1}), *partialBytes),
        "the independent injector bypasses the complete-only publication contract");
  const auto unqualified =
      (*store)->LoadProduct(plan->Definition, plan->InputBinding, kTransportFixtureLimits);
  CHECK(unqualified && !*unqualified, "even a valid partial package cannot satisfy a full lookup");
  CHECK((*store)->Store(*plan, bounds, kTransportFixtureLimits),
        "a completed retry repairs the same content address");
  const auto repaired =
      (*store)->LoadProduct(plan->Definition, plan->InputBinding, kTransportFixtureLimits);
  CHECK(repaired && *repaired && (*store)->BindRequest(address, **repaired),
        "only the repaired complete product can be bound");
  auto other = address;
  ++other.Definition.Core.X;
  const auto otherKey = TransportPlanRequestKey(other);
  CHECK(otherKey && (*raw)->BindRequest(record->Key, *otherKey),
        "the independent injector points a different core at the complete product");
  const auto mismatch = (*store)->LoadRequest(other, kTransportFixtureLimits);
  CHECK(!mismatch && mismatch.error() == TransportPlanAssetError::WrongProduct,
        "a persisted alias cannot change the geographic definition of its product");
  const auto retained = (*store)->LoadRequest(address, kTransportFixtureLimits);
  CHECK(retained && *retained, "rejecting a wrong alias preserves the valid product and request");
}
}

int main() {
  const auto directory = std::filesystem::temp_directory_path() /
                         ("outshine-engine-transport-faults-" + std::to_string(getpid()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "fault injection has an isolated system temporary directory");
  if (error) { return outshine::Test::Report(); }
  Exercise(directory.string());
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the private fault store is removed");
  return outshine::Test::Report();
}
