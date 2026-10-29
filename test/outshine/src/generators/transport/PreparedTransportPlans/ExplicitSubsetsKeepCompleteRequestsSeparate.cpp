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

void CheckSeparation(PreparedTransportPlans &store,
                     const TransportPlan &complete,
                     const TransportPlan &subset,
                     const Box &bounds) {
  const TransportPlanAddress fullAddress{.Definition = complete.Definition,
                                         .Inputs = TransportInputScopeId(complete.InputBinding)};
  const TransportPlanAddress subsetAddress{.Definition = subset.Definition,
                                           .Inputs = TransportInputScopeId(subset.InputBinding)};
  const auto before = store.LoadRequest(subsetAddress, kTransportFixtureLimits);
  CHECK(before && !*before, "the explicit subset request misses before publication");
  const auto fullRecord = store.Store(complete, bounds, kTransportFixtureLimits);
  const auto subsetRecord = store.Store(subset, bounds, kTransportFixtureLimits);
  CHECK(fullRecord && subsetRecord && fullRecord->Key != subsetRecord->Key,
        "the same resolved inputs produce distinct complete and subset content addresses");
  if (!fullRecord || !subsetRecord) { return; }
  const auto full =
      store.LoadProduct(complete.Definition, complete.InputBinding, kTransportFixtureLimits);
  const auto part =
      store.LoadProduct(subset.Definition, subset.InputBinding, kTransportFixtureLimits);
  CHECK(full && *full && part && *part, "both explicitly different functional contracts restore");
  if (!full || !*full || !part || !*part) { return; }
  CHECK((**part).Plan->Coverage == TransportCoverage::Partial &&
            (**part).Plan->Omissions.size() == subset.Omissions.size() &&
            (**part).Plan->Omissions.front().SourceId == subset.Omissions.front().SourceId &&
            (**part).Plan->Omissions.front().Reason == subset.Omissions.front().Reason,
        "a subset hit retains achieved partial coverage and exact declared omissions");
  const auto wrongFull = store.BindRequest(fullAddress, **part);
  const auto wrongPart = store.BindRequest(subsetAddress, **full);
  CHECK(!wrongFull && wrongFull.error() == TransportPlanAssetError::WrongProduct && !wrongPart &&
            wrongPart.error() == TransportPlanAssetError::WrongProduct,
        "neither request contract accepts the other product as an alias");
  CHECK(store.BindRequest(fullAddress, **full) && store.BindRequest(subsetAddress, **part),
        "validated request aliases use the same existing index under separate representation keys");
  const auto fullHit = store.LoadRequest(fullAddress, kTransportFixtureLimits);
  const auto subsetHit = store.LoadRequest(subsetAddress, kTransportFixtureLimits);
  CHECK(fullHit && *fullHit && subsetHit && *subsetHit &&
            (**fullHit).Plan->Coverage == TransportCoverage::Complete &&
            (**subsetHit).Plan->Coverage == TransportCoverage::Partial,
        "the complete hit remains complete after publishing and binding a qualified subset");
  auto otherInputs = subsetAddress;
  otherInputs.Inputs.Bytes[0] ^= uint8_t{1};
  const auto unrelated = store.LoadRequest(otherInputs, kTransportFixtureLimits);
  CHECK(unrelated && !*unrelated,
        "different known inputs cannot reuse a previously qualified subset request");
  auto tiny = kTransportFixtureLimits;
  tiny.DecodedCapacityMost = sizeof(TransportPlan) - 1;
  const auto capped = store.LoadRequest(subsetAddress, tiny);
  CHECK(!capped && capped.error() == TransportPlanAssetError::CapacityExceeded,
        "a persisted subset does not bypass decoded residency admission");
  auto otherCore = subsetAddress;
  ++otherCore.Definition.Core.X;
  CHECK(!store.BindRequest(otherCore, **part),
        "a matching subset guarantee cannot bind a different core");
  auto future = subsetAddress;
  ++future.Definition.RepresentationVersion;
  const auto unsupported = store.LoadRequest(future, kTransportFixtureLimits);
  CHECK(!unsupported && unsupported.error() == TransportPlanAssetError::InvalidRequest,
        "an unknown subset version is not a fallback alias");
}

void CheckAdmission(PreparedTransportPlans &store, TransportPlan subset, const Box &bounds) {
  auto falseComplete = subset;
  falseComplete.Coverage = TransportCoverage::Complete;
  falseComplete.Omissions.clear();
  const auto rejected = store.Store(falseComplete, bounds, kTransportFixtureLimits);
  CHECK(!rejected && rejected.error() == TransportPlanAssetError::InvalidPlan,
        "a subset contract cannot be silently promoted to a complete result");
  subset.Definition.Representation = TransportRepresentation::Functional;
  subset.Definition.RepresentationVersion = 2;
  const auto incomplete = store.Store(subset, bounds, kTransportFixtureLimits);
  CHECK(!incomplete && incomplete.error() == TransportPlanAssetError::InvalidPlan,
        "partial data under the original complete request contract remains rejected");
  subset.Definition.Representation = TransportRepresentation::FunctionalSubset;
  subset.Definition.RepresentationVersion = 1;
  std::stop_source stopped;
  stopped.request_stop();
  const auto canceled = store.Store(subset, bounds, kTransportFixtureLimits, stopped.get_token());
  CHECK(!canceled && canceled.error() == TransportPlanAssetError::Canceled,
        "a canceled subset cannot publish a new package");
  auto tiny = kTransportFixtureLimits;
  tiny.PackageBytesMost = 1;
  const auto capped = store.Store(subset, bounds, tiny);
  CHECK(!capped && capped.error() == TransportPlanAssetError::CapacityExceeded,
        "subset publication honors the same package admission bound");
  subset.Omissions.clear();
  CHECK(!store.Store(subset, bounds, kTransportFixtureLimits),
        "partial coverage without explicit omissions is still invalid");
  subset.Omissions.push_back({.SourceId = "fixture/known-unqualified",
                              .Reason = TransportOmissionReason::UnsupportedRepresentation});
  subset.Network.reset();
  const auto geometryOnly = store.Store(subset, bounds, kTransportFixtureLimits);
  CHECK(!geometryOnly && geometryOnly.error() == TransportPlanAssetError::InvalidPlan,
        "a geometric-only partial product cannot satisfy the explicit functional subset contract");
}

void CheckCorruption(PreparedTransportPlans &store,
                     const std::string &directory,
                     const TransportPlan &subset) {
  auto raw = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  auto stored = store.LoadProduct(subset.Definition, subset.InputBinding, kTransportFixtureLimits);
  auto bytes = EncodeTransportPlan(subset, kTransportFixtureLimits);
  CHECK(raw && stored && *stored && bytes, "a valid subset backs the independent fault injection");
  if (!raw || !stored || !*stored || !bytes) { return; }
  (*bytes)[0] ^= uint8_t{1};
  auto record = (**stored).Record;
  record.Package.clear();
  record.ByteCount = bytes->size();
  CHECK((*raw)->Publish(std::span(&record, size_t{1}), *bytes),
        "the injector persists malformed subset bytes with a valid package checksum");
  const auto corrupt =
      store.LoadProduct(subset.Definition, subset.InputBinding, kTransportFixtureLimits);
  CHECK(corrupt && !*corrupt, "a corrupt subset package cannot become a Ready plan lease");
}
}

int main() {
  const auto directory = std::filesystem::temp_directory_path() /
                         ("outshine-transport-subset-" + std::to_string(getpid()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "the explicit request contracts have a private asset store");
  if (error) { return Report(); }
  {
    auto store = PreparedTransportPlans::Open(directory.string());
    auto full = TransportPlanFixture();
    CHECK(store && full,
          "the existing qualified full fixture and asset implementation are available");
    if (store && full) {
      auto subset = *full;
      subset.Definition.Representation = TransportRepresentation::FunctionalSubset;
      subset.Definition.RepresentationVersion = 1;
      subset.Coverage = TransportCoverage::Partial;
      subset.Omissions.push_back({.SourceId = "fixture/known-unqualified",
                                  .Reason = TransportOmissionReason::UnsupportedRepresentation});
      const Box bounds{.Min = {{-1, -1, -1}}, .Max = {{1, 1, 1}}};
      CheckSeparation(**store, *full, subset, bounds);
      CheckAdmission(**store, subset, bounds);
      CheckCorruption(**store, directory.string(), subset);
    }
  }
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the private subset store is removed after its owners release");
  return Report();
}
