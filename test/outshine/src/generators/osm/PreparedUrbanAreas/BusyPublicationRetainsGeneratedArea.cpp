#include "UrbanAssetFixture.h"
#include "AssetCache.h"
#include "Check.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <print>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <utility>

namespace {
using namespace UrbanAssetFixture;
using namespace outshine::Test;

std::optional<std::string> Seed(const std::filesystem::path &directory) {
  const Fixture fixture(directory, Availability::Ready);
  if (!fixture.Urban) { return std::nullopt; }
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  const auto ready = Await(fixture, request);
  CHECK(ready && ready->Product, "the original complete native area is generated");
  if (!ready || !ready->Product) { return std::nullopt; }
  CHECK(CompleteArea(*ready->Product), "the seeded area has usable geometry and provenance");
  const auto cache = AssetCache::Open((directory / "assets.sqlite").string());
  CHECK(cache && (*cache)->Remove(ready->AssetKey),
        "only the area is removed; both native source products remain available");
  return cache ? std::optional(ready->AssetKey) : std::nullopt;
}

void Resume(const std::filesystem::path &directory) {
  const auto expected = Seed(directory);
  if (!expected) { return; }
  const Fixture fixture(directory, Availability::Unavailable);
  if (!fixture.Urban) { return; }
  sqlite3 *raw = nullptr;
  CHECK(sqlite3_open((directory / "assets.sqlite").c_str(), &raw) == SQLITE_OK,
        "the independent writer opens the actual private asset store");
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> writer(raw, sqlite3_close);
  if (!writer) { return; }
  CHECK(sqlite3_exec(writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "another connection holds publication admission");
  const PreparedUrbanAreas::Request first(kOwner, MvtSchema::OpenMapTiles);
  const PreparedUrbanAreas::Request second(kOwner, MvtSchema::OpenMapTiles);
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
  do {
    const auto a = fixture.Urban->Acquire(first);
    const auto b = fixture.Urban->Acquire(second);
    CHECK(a && b && a->Status == PreparedUrbanAreas::State::Pending &&
              b->Status == PreparedUrbanAreas::State::Pending,
          "writer contention retains both shared native area requests");
    if (!a || !b) {
      std::println("urban publication failed: {}", !a ? a.error() : b.error());
      return;
    }
    (void)fixture.Urban->AwaitProgress(0.001);
  } while (std::chrono::steady_clock::now() < until);
  CHECK(fixture.Calls == 0, "publication contention does not reacquire unavailable sources");
  CHECK(sqlite3_exec(writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the competing writer releases publication admission");
  const auto ready = Await(fixture, second);
  CHECK(ready && ready->Product, "the retained area is published after contention ends");
  if (!ready || !ready->Product) { return; }
  CHECK(ready->AssetKey == *expected && CompleteArea(*ready->Product) && fixture.Calls == 0,
        "resumed publication supplies the same usable asset without reacquiring sources");
}

void CancelRetained(const std::filesystem::path &directory) {
  if (!Seed(directory)) { return; }
  Fixture fixture(directory, Availability::Unavailable);
  if (!fixture.Urban) { return; }
  sqlite3 *raw = nullptr;
  CHECK(sqlite3_open((directory / "assets.sqlite").c_str(), &raw) == SQLITE_OK,
        "the cancellation writer opens");
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> writer(raw, sqlite3_close);
  if (!writer) { return; }
  CHECK(sqlite3_exec(writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the writer retains the cancellation candidate before publication");
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
  while (std::chrono::steady_clock::now() < until) {
    const auto result = fixture.Urban->Acquire(request);
    CHECK(result && result->Status == PreparedUrbanAreas::State::Pending,
          "publication remains pending while the writer owns admission");
    if (!result) { return; }
    (void)fixture.Urban->AwaitProgress(0.001);
  }
  const auto barrier = fixture.Compute.Post([] {});
  fixture.Compute.Wait(barrier);
  fixture.Urban->Cancel();
  const auto cleanup = fixture.Compute.Post([] {});
  fixture.Compute.Wait(cleanup);
  CHECK(!fixture.Urban->Acquire(request),
        "cancelled publication rejects later requests after worker cleanup");
  CHECK(sqlite3_exec(writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the cancellation writer releases its lock");
}

void PermanentFailure(const std::filesystem::path &directory) {
  if (!Seed(directory)) { return; }
  const Fixture fixture(directory, Availability::Unavailable);
  if (!fixture.Urban) { return; }
  sqlite3 *raw = nullptr;
  CHECK(sqlite3_open((directory / "assets.sqlite").c_str(), &raw) == SQLITE_OK,
        "the constraint owner opens");
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> writer(raw, sqlite3_close);
  if (!writer) { return; }
  CHECK(
      sqlite3_exec(writer.get(),
                   "CREATE TRIGGER reject_area BEFORE INSERT ON assets WHEN NEW.kind='urban-area' "
                   "BEGIN SELECT RAISE(ABORT,'fixture failure'); END",
                   nullptr,
                   nullptr,
                   nullptr) == SQLITE_OK,
      "a permanent native constraint rejects only area publication");
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  const auto failed = Await(fixture, request);
  CHECK(!failed && !failed.error().empty(),
        "a permanent publication error remains an explicit failure");
}

}

int main() {
  const std::filesystem::path directory = PlantedPath("urban-publication-contention");
  std::filesystem::remove_all(directory);
  Resume(directory / "resume");
  CancelRetained(directory / "cancel");
  PermanentFailure(directory / "permanent");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the isolated native cache is removed");
  return Report();
}
