#include "OpenAssetStore.h"
#include "UrbanAssetFixture.h"
#include "JobAdmission.h"
#include "Check.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace {
using namespace UrbanAssetFixture;
using namespace outshine::Test;

void DeferredLookup(const std::filesystem::path &directory, bool warm) {
  Fixture fixture(directory, Availability::Ready);
  if (!fixture.Urban) { return; }
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  std::string key;
  if (warm) {
    const auto seeded = Await(fixture, request);
    CHECK(seeded && seeded->Status == PreparedUrbanAreas::State::Ready,
          "warm fixture contains a complete native product");
    if (!seeded) { return; }
    key = seeded->AssetKey;
  }
  fixture.Urban.reset();
  const auto calls = fixture.Calls.load();
  const auto connections = std::make_shared<JobAdmission>(1);
  auto held = connections->TryAcquire();
  CHECK(held, "another cache owner occupies the sole runtime connection");
  auto opened = PreparedUrbanAreas::Open(
      outshine::Test::OpenAssetStore(directory.string(),
                                     {.Workers = &fixture.Compute, .Connections = connections}),
      fixture.Session->Sources(),
      fixture.Compute,
      fixture.Tiles,
      {});
  CHECK(opened, "runtime configuration needs no free connection");
  if (!opened) { return; }
  fixture.Urban = std::move(*opened);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  auto pending = fixture.Urban->Acquire(request);
  while (pending && fixture.Urban->Costs().Deferred < 3 &&
         std::chrono::steady_clock::now() < deadline) {
    (void)fixture.Urban->AwaitProgress(0.001);
    pending = fixture.Urban->Acquire(request);
  }
  CHECK(pending && pending->Status == PreparedUrbanAreas::State::Pending &&
            fixture.Urban->Costs().Deferred >= 3,
        "busy lookup remains pending instead of failing or falling through to sources");
  const auto costs = fixture.Urban->Costs();
  CHECK(costs.SourcePolls == 0 && costs.Misses == 0 && costs.Generations == 0 &&
            fixture.Calls.load() == calls,
        "connection pressure cannot create a cache miss or source work");
  held.reset();
  const auto ready = Await(fixture, request);
  CHECK(ready && ready->Status == PreparedUrbanAreas::State::Ready && ready->Product,
        "the same request progresses after the other owner releases its connection");
  const auto finished = fixture.Urban->Costs();
  if (warm) {
    CHECK(ready && ready->AssetKey == key && finished.Hits == 1 && finished.Misses == 0 &&
              finished.Generations == 0 && finished.SourcePolls == 0 &&
              fixture.Calls.load() == calls,
          "deferred warm hit preserves identity without source acquisition or regeneration");
  } else {
    CHECK(finished.Misses == 1 && finished.Generations == 1 && finished.Writes == 1,
          "deferred cold request generates and publishes exactly once");
  }
}

void StorageFailure(const std::filesystem::path &directory) {
  Fixture fixture(directory / "valid", Availability::Ready);
  if (!fixture.Urban) { return; }
  const auto blocked = directory / "file";
  {
    std::ofstream file(blocked);
    file << "not a directory";
  }
  const auto connections = std::make_shared<JobAdmission>(1);
  auto opened = PreparedUrbanAreas::Open(
      outshine::Test::OpenAssetStore(blocked.string(),
                                     {.Workers = &fixture.Compute, .Connections = connections}),
      fixture.Session->Sources(),
      fixture.Compute,
      fixture.Tiles,
      {});
  CHECK(opened, "deferred runtime opens do not inspect storage on the caller");
  if (!opened) { return; }
  fixture.Urban = std::move(*opened);
  const auto failed = Await(fixture, {kOwner, MvtSchema::OpenMapTiles});
  CHECK(!failed && fixture.Urban->Costs().Deferred == 0 &&
            fixture.Urban->Costs().SourcePolls == 0 && connections->Active() == 0,
        "real storage failure stays terminal and releases connection admission");
}
}

int main() {
  using namespace outshine::Test;
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-urban-busy-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  DeferredLookup(directory / "cold", false);
  DeferredLookup(directory / "warm", true);
  StorageFailure(directory / "storage");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated native fixtures are removed");
  return Report();
}
