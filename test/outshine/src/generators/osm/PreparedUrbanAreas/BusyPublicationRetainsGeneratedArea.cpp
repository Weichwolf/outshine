#include "UrbanAssetFixture.h"
#include "UrbanAreaCodec.h"
#include "AssetCache.h"
#include "Check.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <utility>
#include <vector>

namespace {
using namespace UrbanAssetFixture;
using namespace outshine::Test;
constexpr auto kWait = std::chrono::milliseconds(1);
constexpr auto kDeadline = std::chrono::seconds(10);
using Writer = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

Writer OpenWriter(const std::filesystem::path &root) {
  sqlite3 *raw = nullptr;
  CHECK(sqlite3_open((root / "assets.sqlite").c_str(), &raw) == SQLITE_OK,
        "an independent SQLite owner opens the real private store");
  return {raw, sqlite3_close};
}

std::optional<std::vector<uint8_t>> SeedSources(const std::filesystem::path &root) {
  Fixture fixture(root, Availability::Ready);
  if (!fixture.Urban) { return std::nullopt; }
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  const auto result = Await(fixture, request);
  CHECK(result && result->Product, "the actual owner and parent sources produce a native area");
  if (!result || !result->Product) { return std::nullopt; }
  auto bytes = EncodeUrbanArea(*result->Product);
  auto cache = AssetCache::Open((root / "assets.sqlite").string());
  CHECK(cache && bytes, "the original complete native result and store remain available");
  if (!cache || !bytes) { return std::nullopt; }
  CHECK((*cache)->Remove(result->AssetKey),
        "only the private urban product is removed; both native source products remain");
  return std::move(*bytes);
}

void Resume(const std::filesystem::path &root) {
  const auto expected = SeedSources(root);
  if (!expected) { return; }
  Fixture fixture(root, Availability::Unavailable, {.PublicationBytesMost = 1});
  if (!fixture.Urban) { return; }
  auto writer = OpenWriter(root);
  if (!writer) { return; }
  CHECK(sqlite3_exec(writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "another writer holds atomic publication admission");
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (fixture.Urban->Costs().PublicationDeferred == 0 &&
         std::chrono::steady_clock::now() < until) {
    const auto result = fixture.Urban->Acquire(*fixture.Pool, request);
    CHECK(result && result->Status == PreparedUrbanAreas::State::Pending,
          "writer contention keeps valid native area preparation pending");
    if (!result) { return; }
    (void)fixture.Urban->AwaitProgress(std::chrono::duration<double>(kWait).count());
  }
  const auto held = fixture.Urban->Costs();
  CHECK(held.Generations == 1 && held.PublicationBytes > 0 && held.PublicationDeferred > 0 &&
            held.Writes == 0 && fixture.Calls == 0,
        "one generated area survives busy admission without reopening the provider");
  const PreparedUrbanAreas::Request second(
      kOwner, MvtSchema::OpenMapTiles, {.ProjectedPitchM = 64});
  for (unsigned turn = 0; turn < 32; ++turn) {
    const auto first = fixture.Urban->Acquire(*fixture.Pool, request);
    const auto another = fixture.Urban->Acquire(*fixture.Pool, second);
    CHECK(first && another && first->Status == PreparedUrbanAreas::State::Pending &&
              another->Status == PreparedUrbanAreas::State::Pending,
          "retained capacity defers new generation and incomplete publication");
    if (!first || !another) { return; }
    (void)fixture.Urban->AwaitProgress(std::chrono::duration<double>(kWait).count());
  }
  CHECK(fixture.Urban->Costs().Generations == 1 && fixture.Calls == 0,
        "polling under contention neither regenerates nor fetches another source");
  CHECK(sqlite3_exec(writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the competing writer releases admission");
  const auto resumed = Await(fixture, request);
  CHECK(resumed && resumed->Product, "the retained native product commits and restores");
  if (!resumed || !resumed->Product) { return; }
  const auto actual = EncodeUrbanArea(*resumed->Product);
  CHECK(actual && *actual == *expected,
        "resumed native content is byte-identical to the original complete area");
  const auto following = Await(fixture, second);
  CHECK(following && following->Product && fixture.Urban->Costs().Generations == 2 &&
            fixture.Urban->Costs().Writes == 2 && fixture.Urban->Costs().PublicationBytes == 0 &&
            fixture.Calls == 0,
        "released capacity admits the next native generation and releases every package");
  Fixture warm(root, Availability::Unavailable);
  if (!warm.Urban) { return; }
  CHECK(sqlite3_exec(writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "another writer remains active during a fresh native hit");
  const auto restored = Await(warm, request);
  CHECK(restored && restored->Product && warm.Urban->Costs().Hits == 1 &&
            warm.Urban->Costs().Generations == 0 && warm.Calls == 0 &&
            warm.Urban->Costs().SourcePolls == 0,
        "a fresh warm owner reads the committed product without source work under a writer");
  CHECK(sqlite3_exec(writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the warm-hit writer releases admission");
}

void Cancel(const std::filesystem::path &root) {
  if (!SeedSources(root)) { return; }
  Fixture fixture(root, Availability::Unavailable);
  if (!fixture.Urban) { return; }
  auto writer = OpenWriter(root);
  if (!writer) { return; }
  CHECK(sqlite3_exec(writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "cancellation starts under actual competing writer admission");
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (fixture.Urban->Costs().PublicationDeferred == 0 &&
         std::chrono::steady_clock::now() < until) {
    const auto result = fixture.Urban->Acquire(*fixture.Pool, request);
    CHECK(result, "valid preparation remains pending before cancellation");
    if (!result) { return; }
    (void)fixture.Urban->AwaitProgress(std::chrono::duration<double>(kWait).count());
  }
  CHECK(fixture.Urban->Costs().PublicationBytes > 0,
        "a real prepared urban publication waits when its last owner is destroyed");
  const std::weak_ptr<PreparedUrbanAreas> observed = fixture.Urban;
  fixture.Urban.reset();
  CHECK(observed.expired(), "destruction joins running work and releases deferred publication");
  CHECK(sqlite3_exec(writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "writer admission resumes after native cancellation");
  sqlite3_stmt *rows = nullptr;
  CHECK(sqlite3_prepare_v2(writer.get(),
                           "SELECT count(*) FROM assets WHERE kind='urban-area'",
                           -1,
                           &rows,
                           nullptr) == SQLITE_OK,
        "the independent writer inspects only urban product rows");
  if (rows != nullptr) {
    CHECK(sqlite3_step(rows) == SQLITE_ROW && sqlite3_column_int64(rows, 0) == 0,
          "cancelled retained work never commits an urban product after its owner disappears");
  }
  sqlite3_finalize(rows);
}

void RejectPermanentFailure(const std::filesystem::path &root) {
  if (!SeedSources(root)) { return; }
  Fixture fixture(root, Availability::Unavailable);
  if (!fixture.Urban) { return; }
  auto writer = OpenWriter(root);
  if (!writer) { return; }
  CHECK(sqlite3_exec(writer.get(),
                     "CREATE TRIGGER reject_native_write BEFORE INSERT ON assets "
                     "WHEN NEW.kind='urban-area' "
                     "BEGIN SELECT RAISE(ABORT, 'fixture storage failure'); END",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "an actual storage constraint rejects only native urban publication");
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  const auto result = Await(fixture, request);
  const auto cost = fixture.Urban->Costs();
  CHECK(!result && cost.Generations == 1 && cost.PublicationAttempts == 1 &&
            cost.PublicationDeferred == 0 && cost.PublicationBytes == 0 && cost.Writes == 0,
        "permanent native storage errors stop and release their package without busy retries");
}
}

int main() {
  const std::filesystem::path root(PlantedPath("busy-native-urban-publication"));
  std::filesystem::create_directories(root);
  Resume(root / "resume");
  Cancel(root / "cancel");
  RejectPermanentFailure(root / "permanent");
  std::filesystem::remove_all(root);
  return Report();
}
