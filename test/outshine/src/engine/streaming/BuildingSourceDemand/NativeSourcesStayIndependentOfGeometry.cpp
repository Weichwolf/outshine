#include "test/harness/engine/NativeBuildingSources.h"
#include "BuildingSourceDemand.h"
#include "BuiltinSourceProviders.h"
#include "test/outshine/src/engine/streaming/StructureBuildQueue/BuildingQueueFixture.h"
#include "BuildingInputs.h"
#include "HeightField.h"
#include "Check.h"
#include "Tasks.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <sqlite3.h>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::BuildingReplay;

using namespace outshine::Test::BuildingSources;

void MissingThenPublished(Fixture &fixture) {
  BuildingSourceDemand demand;
  const auto before = fixture.Stack.BuildingAssets()->Costs();
  const auto streets = fixture.Stack.Ways().LookedCount();
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Missing),
        "a pure native basis miss terminates without requesting geometry");
  const auto absent = fixture.Stack.BuildingAssets()->Costs();
  CHECK(fixture.Queue.Queued() == 0 && fixture.Stack.Ways().LookedCount() == streets &&
            absent.ReadBytes == before.ReadBytes &&
            absent.SurfaceReadBytes == before.SurfaceReadBytes &&
            absent.GeometryReadBytes == before.GeometryReadBytes,
        "metadata misses start no source forms, geometry, height pins or street ingestion");
  const auto generated = fixture.Generate();
  CHECK(generated.has_value(), "the ordinary native producer publishes actual MVT building inputs");
  if (!generated) { return; }
  const auto published = fixture.Stack.BuildingAssets()->Costs();
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Ready),
        "a newly published basis binding makes a previous metadata miss discoverable");
  const auto &entry = demand.Entries()[0];
  CHECK(entry.Value && entry.Value->BaseKey == *generated && !entry.Value->Bounds.Empty() &&
            entry.Value->Cells != 0,
        "the compact descriptor retains native content, ECEF bounds and occupied cells");
  const auto ready = fixture.Stack.BuildingAssets()->Costs();
  CHECK(ready.ReadBytes == published.ReadBytes &&
            ready.SurfaceReadBytes == published.SurfaceReadBytes &&
            ready.GeometryReadBytes == published.GeometryReadBytes,
        "discovering a ready source reads only the basis and request metadata");
  for (size_t turn = 0; turn < 10; ++turn) {
    (void)demand.Advance(fixture.Queue, fixture.Stack, 1);
  }
  CHECK(fixture.Stack.BuildingAssets()->Costs().BasisReadBytes == ready.BasisReadBytes,
        "unchanged World demand does not repeatedly decode the immutable basis");
  CHECK(demand.HeapBytes() > 0 && fixture.Queue.Queued() == 0,
        "World retains a compact source descriptor without live geometry jobs");
}

void UnavailableIsNotMissing(Fixture &fixture) {
  StructureBuildQueue unopened;
  BuildingSourceDemand demand;
  CHECK(demand.Advance(unopened, fixture.Stack, 1) && demand.Counts().Pending == 1 &&
            demand.Counts().Missing == 0 && unopened.Queued() == 0,
        "unconfigured execution remains unknown and does not become a cached miss");
}

void CoalescedAndBounded(Fixture &fixture) {
  std::atomic_bool released{false};
  const auto blocker = fixture.Worker.Post([&released] { released.wait(false); });
  const auto before = fixture.Stack.BuildingAssets()->Costs();
  const auto first = fixture.Queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
  const auto again = fixture.Queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
  CHECK(first && again && first->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending &&
            again->RequestKey == first->RequestKey && fixture.Queue.Queued() == 1,
        "identical callers share one bounded worker lookup");
  CHECK(fixture.Stack.BuildingAssets()->Costs().BasisReadBytes == before.BasisReadBytes,
        "a held worker proves no caller-side basis read or decode");
  for (size_t at = 1; at <= StructureBuildQueue::kCandidateWindow; ++at) {
    fixture.Stack.Footprints().TilesSpan(kSpan + static_cast<double>(at));
    const auto query = fixture.Queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
    CHECK(query && query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
          "new demand remains pending while lookup admission is full");
  }
  CHECK(fixture.Queue.Queued() == StructureBuildQueue::kCandidateWindow,
        "native metadata work never exceeds the existing candidate window");
  released.store(true);
  released.notify_all();
  fixture.Worker.Wait(blocker);
  fixture.Queue.Clear();
  fixture.Stack.Footprints().TilesSpan(kSpan);
}

void CompactLeaseAndCamera(Fixture &fixture) {
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  auto query = fixture.Queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
  while (query && query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    query = fixture.Queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
  }
  CHECK(query && query->Source != nullptr, "the native source offers a borrowed complete basis");
  if (!query || query->Source == nullptr) { return; }
  const std::weak_ptr<const Generators::PreparedBuildingBasis> basis = query->Source->Product;
  BuildingSourceDemand demand;
  CHECK(demand.Advance(fixture.Queue, fixture.Stack, 1) && demand.Counts().Ready == 1,
        "World consumes the borrowed basis into its compact source descriptor");
  CHECK(basis.expired(), "the descriptor retains no full basis or source-coordinate owner");
  const auto before = fixture.Stack.BuildingAssets()->Costs();
  fixture.Stack.Footprints().SeenWith({.FocalPx = 1440});
  fixture.Stack.Footprints().ViewedFrom(
      TangentFrame::At({.LongitudeDeg = 0.011, .LatitudeDeg = 0.012}).OriginEcef());
  CHECK(!demand.Advance(fixture.Queue, fixture.Stack, 1) && demand.Counts().Ready == 1 &&
            fixture.Stack.BuildingAssets()->Costs().BasisReadBytes == before.BasisReadBytes,
        "camera and projected detail changes preserve immutable source demand");
  const auto shape = fixture.Stack.Pool().Shaped();
  auto changed = shape;
  ++changed.Seed;
  fixture.Stack.Pool().Shapes(changed);
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Missing) &&
            !demand.Entries()[0].Value,
        "changed source shape cannot retain the previous source descriptor");
  fixture.Stack.Pool().Shapes(shape);
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Ready),
        "restoring exact source inputs restores their pinned asset binding");
}

void FreshWarmAndStoreScope(Fixture &original) {
  Fixture warm;
  CHECK(warm.Open(original.Directory.Path, true), "a fresh World opens cached native inputs");
  if (!warm.Stack.Opened() || warm.Stack.Vectors() == nullptr) { return; }
  BuildingSourceDemand demand;
  const auto before = warm.Stack.BuildingAssets()->Costs();
  CHECK(WaitFor(demand, warm, BuildingSourceDemand::Phase::Ready),
        "fresh World metadata does not depend on a prior geometry landing or street plan");
  const auto after = warm.Stack.BuildingAssets()->Costs();
  CHECK(warm.Stack.Ways().LookedCount() == 0 && after.ReadBytes == before.ReadBytes &&
            after.SurfaceReadBytes == before.SurfaceReadBytes &&
            after.GeometryReadBytes == before.GeometryReadBytes,
        "cached World source discovery never enters the geometry or street fallback");
  const auto foreign = warm.Queue.PollNativeBasis(original.Stack, original.Stack.Footprints(), 0);
  const auto *failure =
      foreign ? nullptr : std::get_if<Generators::StructureBakeErrorKind>(&foreign.error());
  CHECK(failure && *failure == Generators::StructureBakeErrorKind::ArtifactInvalidProduct,
        "a queue cannot return another asset store's request binding");
  warm.Queue.Clear();
  const auto rebound = warm.Queue.PollNativeBasis(original.Stack, original.Stack.Footprints(), 0);
  CHECK(rebound && rebound->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
        "clearing a queue permits a new store scope and a fresh bounded lookup");
  warm.Queue.Clear();
  warm.Stack.Close();
  CHECK(demand.Advance(warm.Queue, warm.Stack, 1) && demand.Entries().empty(),
        "unavailable current inputs invalidate previously ready source descriptions");
}

void RepublishedSources() {
  Fixture fixture;
  CHECK(fixture.Open(), "the World source revision fixture opens");
  if (!fixture.Stack.Opened()) { return; }
  const auto original = fixture.Generate(11);
  CHECK(original, "the first native source revision publishes");
  if (!original) { return; }
  BuildingSourceDemand demand;
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Ready),
        "the first native source becomes a ready World descriptor");
  if (demand.Entries().empty()) { return; }
  const auto &first = demand.Entries()[0].Value;
  if (!first) { return; }
  const auto request = first->RequestKey;
  const auto replacement = fixture.Generate(12);
  CHECK(replacement && replacement != original, "a changed native input publishes another basis");
  if (!replacement) { return; }
  const auto binding = fixture.Stack.BuildingAssets()->LoadBasisRequest(request);
  CHECK(binding && *binding && (**binding).BaseKey == *replacement,
        "the actual persisted request now binds the replacement source");
  const auto before = fixture.Stack.BuildingAssets()->Costs();
  CHECK(demand.Advance(fixture.Queue, fixture.Stack, 0) && demand.Counts().Ready == 0 &&
            !demand.Entries()[0].Value,
        "a new publication invalidates a ready descriptor even without lookup admission");
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Ready),
        "bounded World discovery reads the replacement source metadata");
  const auto &latest = demand.Entries()[0].Value;
  CHECK(latest && latest->BaseKey == *replacement && latest->RequestKey == request,
        "a stable request address cannot keep an obsolete ready content identity");
  const auto after = fixture.Stack.BuildingAssets()->Costs();
  CHECK(after.SurfaceReadBytes == before.SurfaceReadBytes &&
            after.GeometryReadBytes == before.GeometryReadBytes,
        "revision discovery opens no source forms or geometry");
}

void ReboundLookup(bool present, bool collected) {
  Fixture fixture;
  CHECK(fixture.Open(), "the completion-order fixture opens");
  if (!fixture.Stack.Opened()) { return; }
  if (present) { CHECK(fixture.Generate(31), "the first source exists before its metadata read"); }
  StructureBuildQueue queue;
  queue.Opens(&fixture.Worker, nullptr);
  const auto pending = queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
  using Phase = StructureBuildQueue::NativeBasisQuery::Phase;
  CHECK(pending && pending->Where == Phase::Pending, "the real request lookup runs off thread");
  const auto barrier = fixture.Worker.Post([] {});
  fixture.Worker.Wait(barrier);
  if (collected) {
    const auto previous = queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
    CHECK(previous && previous->Where == (present ? Phase::Ready : Phase::Missing),
          "the first completed result stays in the ordinary queue cache");
  }
  const auto replacement = fixture.Generate(32);
  CHECK(replacement, "a newer request binding publishes after the old metadata read finished");
  if (!replacement) { return; }
  const auto before = fixture.Stack.BuildingAssets()->Costs();
  const auto changed = queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
  CHECK(changed && changed->Where == Phase::Pending,
        "an observed newer publication prevents an old completed lookup from becoming current");
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  std::optional<std::string> latest;
  do {
    const auto next = queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
    if (!next) { break; }
    if (next->Where == Phase::Ready) {
      latest = next->Source->BaseKey;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  CHECK(latest == replacement, "the next completed request identifies the latest native basis");
  const auto after = fixture.Stack.BuildingAssets()->Costs();
  CHECK(after.SurfaceReadBytes == before.SurfaceReadBytes &&
            after.GeometryReadBytes == before.GeometryReadBytes && queue.Queued() == 0,
        "revalidating request metadata adds no form IO or geometry jobs");
}

void FailedLookupRecovers() {
  Fixture fixture;
  CHECK(fixture.Open(), "the failed metadata fixture opens");
  if (!fixture.Stack.Opened()) { return; }
  CHECK(fixture.Generate(41), "the original native source publishes before the read fault");
  sqlite3 *connection = nullptr;
  const int opened =
      sqlite3_open((fixture.Directory.Path / "assets/assets.sqlite").c_str(), &connection);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(connection, sqlite3_close);
  CHECK(opened == SQLITE_OK && database, "the independent fixture database connection opens");
  if (opened != SQLITE_OK || !database) { return; }
  const int hidden = sqlite3_exec(database.get(),
                                  "ALTER TABLE asset_requests RENAME TO held_requests",
                                  nullptr,
                                  nullptr,
                                  nullptr);
  CHECK(hidden == SQLITE_OK, "a real database read fault replaces the request table temporarily");
  if (hidden != SQLITE_OK) { return; }
  BuildingSourceDemand demand;
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Failed),
        "the actual asynchronous request read exposes the database failure");
  CHECK(sqlite3_exec(database.get(),
                     "ALTER TABLE held_requests RENAME TO asset_requests",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "the fixture restores the original request table");
  const auto replacement = fixture.Generate(42);
  CHECK(replacement, "a valid new native revision publishes after the read fault");
  if (!replacement) { return; }
  CHECK(demand.Advance(fixture.Queue, fixture.Stack, 0) && demand.Counts().Failed == 0,
        "new publication invalidates a previously failed World descriptor");
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Ready),
        "the failed queue lookup is replaced by a current successful read");
  const auto &latest = demand.Entries()[0].Value;
  CHECK(latest && latest->BaseKey == *replacement,
        "a transient read failure cannot permanently pin an obsolete request result");
}
}

int main() {
  Fixture fixture;
  CHECK(fixture.Open(), "real native vector and asset services open");
  if (!fixture.Stack.Opened() || fixture.Stack.Vectors() == nullptr) { return Report(); }
  UnavailableIsNotMissing(fixture);
  MissingThenPublished(fixture);
  CoalescedAndBounded(fixture);
  CompactLeaseAndCamera(fixture);
  FreshWarmAndStoreScope(fixture);
  RepublishedSources();
  for (const bool present : {false, true}) {
    for (const bool collected : {false, true}) { ReboundLookup(present, collected); }
  }
  FailedLookupRecovers();
  return Report();
}
