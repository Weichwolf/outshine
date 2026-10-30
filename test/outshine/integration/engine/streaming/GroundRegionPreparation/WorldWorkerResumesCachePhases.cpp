#include "test/harness/engine/GroundPublicationFixture.h"
#include "StoreGroundRegion.h"
#include "GroundRegionAsset.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <expected>
#include <future>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace GroundPublication;
using Clock = std::chrono::steady_clock;

void Barrier(Fixture &fixture) {
  const auto marker = fixture.Pool.Post([] {});
  CHECK(marker != Tasks::kNoTask, "unrelated work enters the original compute pool");
  if (marker != Tasks::kNoTask) { fixture.Pool.Wait(marker); }
}

bool DriveHeld(Fixture &fixture, const std::atomic_uint32_t &attempts) {
  const auto until = Clock::now() + std::chrono::milliseconds(50);
  while (Clock::now() < until) {
    const auto value = fixture.Job->Collect();
    CHECK(!value && !fixture.Job->Peek(),
          "a held native cache write never becomes a complete World candidate");
    if (value) { return false; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Barrier(fixture);
  CHECK(attempts > 1 && attempts < 20,
        "caller readmission is bounded and does not occupy a spinning worker");
  return attempts > 1 && attempts < 20;
}

void WarmBindAndPeek() {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "native warm-binding inputs open");
  if (!opened) { return; }
  const auto stored = StoreGroundRegion(*fixture.Cache,
                                        fixture.Key,
                                        GroundRegionBoundsEcef(*fixture.Region),
                                        *fixture.Region,
                                        *fixture.Water);
  CHECK(stored, "a content product commits without the later request binding");
  if (!stored) { return; }
  std::atomic_uint32_t keys{0};
  std::atomic_uint32_t attempts{0};
  auto actual = GroundRegionPreparation::Lookup(
      fixture.Cache, fixture.Request, [&] -> std::expected<std::string, std::string> {
        ++keys;
        return fixture.Key;
      });
  fixture.Hold();
  fixture.Job = std::make_unique<GroundRegionPreparation>(
      fixture.Pool,
      [actual = std::exchange(actual, {}), &attempts](const std::stop_token &stop) mutable {
        ++attempts;
        return actual(stop);
      });
  Barrier(fixture);
  CHECK(!fixture.Job->Peek() && attempts == 1 && keys == 1,
        "the actual lookup holds decoded content while its new alias is busy");
  const auto costs = fixture.Cache->Costs();
  const auto until = Clock::now() + std::chrono::milliseconds(25);
  while (Clock::now() < until) {
    CHECK(!fixture.Job->Peek(), "Peek remains a pure immutable observation of incomplete work");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(attempts == 1, "observation alone cannot enqueue a cache retry");
  if (!DriveHeld(fixture, attempts)) { return; }
  CHECK(keys == 1 && fixture.Cache->Costs().Decodes == costs.Decodes &&
            fixture.Cache->Costs().ReadBytes == costs.ReadBytes,
        "the real worker retains its key and decoded region through repeated alias admission");
  fixture.Release();
  const auto result = fixture.Await();
  CHECK(result && *result && result->value().Loaded && result->value().Key == fixture.Key &&
            keys == 1 && fixture.Cache->Costs().Decodes == costs.Decodes,
        "release returns the actual bound World product without another key computation or decode");
}

void ColdPublication(bool roads, bool cancel) {
  Fixture fixture;
  const bool opened = fixture.Open(roads);
  CHECK(opened, "complete native region/water/road publication inputs open");
  if (!opened) { return; }
  const auto expected = EncodeGroundRegionAsset(*fixture.Region, Regions::PackageBytesMost);
  const std::weak_ptr<const GroundRegionAsset> original = fixture.Region;
  const std::weak_ptr<const Generators::WaterAsset> water = fixture.Water;
  const std::weak_ptr<const Generators::RoadPageManifest> manifest = fixture.Manifest;
  auto actual = GroundRegionPreparation::PrepareStore({.Assets = fixture.Cache,
                                                       .Key = fixture.Key,
                                                       .RequestKey = fixture.Request,
                                                       .Region = fixture.Region,
                                                       .Water = fixture.Water,
                                                       .Manifest = fixture.Manifest,
                                                       .Pages = fixture.Pages});
  fixture.Region.reset();
  fixture.Water.reset();
  fixture.Manifest.reset();
  std::atomic_uint32_t attempts{0};
  fixture.Hold();
  fixture.Job = std::make_unique<GroundRegionPreparation>(
      fixture.Pool,
      [actual = std::exchange(actual, {}), &attempts](const std::stop_token &stop) mutable {
        ++attempts;
        return actual(stop);
      },
      fixture.Pages);
  Barrier(fixture);
  const auto prepared = fixture.Cache->Costs().Preparations;
  CHECK(!fixture.Job->Peek() && manifest.expired() &&
            (roads ? prepared == 0 && !original.expired()
                   : prepared == 1 && original.expired() && water.expired()),
        "manifest dependency metadata and prepared region bytes have distinct owning lifetimes");
  if (!DriveHeld(fixture, attempts)) { return; }
  CHECK(fixture.Cache->Costs().Preparations == prepared,
        "held World publication never re-encodes its prepared native products");
  if (cancel) { fixture.Job->Cancel(); }
  fixture.Release();
  const auto result = fixture.Await();
  CHECK(result && original.expired() && water.expired(),
        "completion or cancellation releases all original generated inputs");
  if (!result) { return; }
  if (cancel) {
    CHECK(!*result && result->error() == "ground region preparation canceled",
          "a canceled pending World publication remains terminal and explicit");
    const auto absent = fixture.Cache->LoadRequest(fixture.Request);
    CHECK(absent && !*absent, "canceled native region publication never binds a later request");
    return;
  }
  CHECK(*result && result->value().Loaded && fixture.Cache->Costs().Preparations == 1,
        "the original World job completes one native region after its dependency commits");
  if (!*result || !result->value().Loaded) { return; }
  const auto encoded =
      EncodeGroundRegionAsset(result->value().Loaded->Region, Regions::PackageBytesMost);
  CHECK(expected && encoded && *expected == *encoded &&
            (roads ? result->value().SurfacePages != nullptr
                   : result->value().SurfacePages == nullptr),
        "the complete result preserves source bytes and restores its referenced native page "
        "manifest");
}

void RunningCancelAndTypedErrors() {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "the active cancellation fixture opens");
  if (!opened) { return; }
  std::promise<void> entered;
  std::promise<void> release;
  const auto running = entered.get_future();
  const auto gate = release.get_future().share();
  fixture.Job = std::make_unique<GroundRegionPreparation>(
      fixture.Pool, [&entered, gate](const std::stop_token &) -> GroundRegionPreparation::Attempt {
        entered.set_value();
        (void)gate.wait_for(std::chrono::seconds(2));
        return std::unexpected(std::string("a permanent error containing busy is not cache Busy"));
      });
  CHECK(running.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
        "cancellation observes a genuinely active worker");
  fixture.Job->Cancel();
  release.set_value();
  const auto canceled = fixture.Await();
  CHECK(canceled && !*canceled && canceled->error() == "ground region preparation canceled",
        "one cancellation request makes active work terminal after cleanup");
  fixture.Job = std::make_unique<GroundRegionPreparation>(
      fixture.Pool, [](const std::stop_token &) -> GroundRegionPreparation::Attempt {
        return std::unexpected(std::string("permanent busy wording"));
      });
  const auto failed = fixture.Await();
  CHECK(failed && !*failed && failed->error() == "permanent busy wording",
        "only typed SQLite Busy is retried; permanent input errors stay terminal regardless of "
        "wording");
}
}

int main() {
  WarmBindAndPeek();
  ColdPublication(false, false);
  ColdPublication(true, false);
  ColdPublication(false, true);
  ColdPublication(true, true);
  RunningCancelAndTypedErrors();
  return Report();
}
