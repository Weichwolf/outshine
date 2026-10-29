#include "test/harness/engine/NativeBuildingSources.h"
#include "BuildingMesh.h"
#include "Check.h"
#include "PreparedBuildingAssets.h"
#include "AssetCache.h"
#include "PreparedStructureCodec.h"
#include "StructureBuildQueue.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <expected>
#include <cstddef>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::BuildingSources;
constexpr auto kWait = std::chrono::milliseconds(1);
using Writer = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

struct Case {
  Generators::BuildingMesh Mesher;
  Fixture Env;
  StructureBuildQueue::HeightSource NoHeights{
      .Sample = {}, .PinField = {}, .Revision = {.Value = 11}};
  Writer Database{nullptr, sqlite3_close};
  std::string Key;
  std::string Request;
  Box Bounds;
  Generators::PreparedBuildingAssets::Counters Before;

  bool Open() {
    CHECK(Env.Open(), "the actual vector/street/cache fixture opens");
    if (!Env.Stack.BuildingAssets() || Env.Stack.Vectors() == nullptr) { return false; }
    Env.Queue.Opens(&Env.Worker, &Mesher);
    Env.Stack.Footprints().SeenWith({.FocalPx = 720});
    const auto &vectors = *Env.Stack.Vectors();
    const auto digest = Env.Stack.Ways().SourceDigest(vectors, 0);
    CHECK(digest, "the runtime content key uses the actual prepared street digest");
    if (!digest) { return false; }
    const auto generated = Env.Generate(*digest);
    CHECK(generated, "the real native producer seeds the content product");
    if (!generated) { return false; }
    Key = *generated;
    const auto &tile = vectors.Tiles().front();
    Request = Env.Stack.BuildingAssets()->RequestKey(
        {.Zoom = tile.Z, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)},
        Env.Stack.Pool().Shaped(),
        kSpan,
        tile.InputDigest);
    const auto original = Env.Stack.BuildingAssets()->LoadBasisRequest(Request);
    CHECK(original && *original, "the seeded native basis has a persistent request");
    if (!original || !*original) { return false; }
    Bounds = (**original).Bounds;
    CHECK(!Bounds.Empty(), "the seeded basis retains complete Earth-centred source bounds");
    sqlite3 *raw = nullptr;
    const auto path = Env.Directory.Path / "assets" / "assets.sqlite";
    const int opened = sqlite3_open(path.c_str(), &raw);
    Database.reset(raw);
    CHECK(opened == SQLITE_OK, "an independent SQLite writer opens the actual shared store");
    if (opened != SQLITE_OK) { return false; }
    auto assets = AssetCache::Open(path.string());
    const auto record = assets ? (*assets)->FindRequest(Request)
                               : std::expected<std::optional<AssetRecord>, AssetCacheError>{
                                     std::unexpected(AssetCacheError::Storage)};
    CHECK(assets && record && *record,
          "the private alias identifies exactly its native basis record");
    if (!assets || !record || !*record) { return false; }
    CHECK((*assets)->Remove((**record).Key),
          "only basis and alias are removed; the complete native index remains");
    const auto retained = (*assets)->Load(Key, size_t{64} * 1024 * 1024);
    CHECK(retained && *retained &&
              Generators::DecodePreparedStructureIndex((**retained).Bytes(),
                                                       size_t{128} * 1024 * 1024),
          "the actual fallback can reconstruct its source basis without provider or height inputs");
    CHECK(sqlite3_exec(raw, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "another connection holds alias publication admission");
    Before = Env.Stack.BuildingAssets()->Costs();
    return true;
  }

  bool WaitHeld() {
    const auto until = std::chrono::steady_clock::now() + kDeadline;
    while (std::chrono::steady_clock::now() < until) {
      (void)Env.Queue.Posts(Env.Stack, Env.Stack.Footprints(), Env.Eye, NoHeights, 1);
      const auto ready =
          Env.Queue.NextLandings(Env.Stack, Env.Stack.Footprints(), Env.Eye, NoHeights, 1);
      CHECK(ready && ready->empty(), "busy alias publication cannot expose a partial landing");
      const auto query = Env.Queue.PollNativeBasis(Env.Stack, Env.Stack.Footprints(), 0);
      CHECK(query && (query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending ||
                      query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Missing),
            "the actual native lookup keeps writer contention pending rather than permanent");
      if (!ready || !query) { return false; }
      (void)Env.Worker.AwaitCompletion(std::chrono::duration<double>(kWait).count());
      if (Env.Stack.BuildingAssets()->Costs().ReadBytes > Before.ReadBytes &&
          Env.Stack.BuildingAssets()->Costs().BasisPreparations > Before.BasisPreparations) {
        return true;
      }
    }
    CHECK(false, "the actual content lookup reaches its decoded basis under a held writer");
    return false;
  }

  void ReleaseWriter() const {
    CHECK(sqlite3_exec(Database.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases admission");
  }
};

void Resume() {
  Case test;
  if (!test.Open() || !test.WaitHeld()) { return; }
  const auto held = test.Env.Stack.BuildingAssets()->Costs();
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
  while (std::chrono::steady_clock::now() < until) {
    const auto query =
        test.Env.Queue.PollNativeBasis(test.Env.Stack, test.Env.Stack.Footprints(), 0);
    CHECK(query && query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
          "retained alias work waits without becoming a ready native source");
    if (!query) { return; }
    (void)test.Env.Worker.AwaitCompletion(std::chrono::duration<double>(kWait).count());
  }
  const auto waiting = test.Env.Stack.BuildingAssets()->Costs();
  CHECK(waiting.ReadBytes == held.ReadBytes &&
            waiting.BasisPreparations == held.BasisPreparations &&
            waiting.BasisWrites == test.Before.BasisWrites &&
            waiting.Writes == test.Before.Writes && test.Env.Queue.Queued() <= 4,
        "retry retains the native fallback input without repeated index decode, generation or "
        "extra slots");
  test.ReleaseWriter();
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  std::optional<StructureBuildQueue::NativeBasisQuery> completed;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto query =
        test.Env.Queue.PollNativeBasis(test.Env.Stack, test.Env.Stack.Footprints(), 0);
    CHECK(query, "writer release resumes the same actual native lookup");
    if (!query) { return; }
    if (query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Ready) {
      completed = *query;
      break;
    }
    (void)test.Env.Worker.AwaitCompletion(std::chrono::duration<double>(kWait).count());
  }
  CHECK(completed && completed->Source && completed->Source->Product &&
            completed->Source->BaseKey == test.Key && completed->Source->Bounds == test.Bounds,
        "committed alias exposes the same native identity and persisted nonempty bounds");
  if (!completed || completed->Source == nullptr) { return; }
  const auto product = completed->Source->Product;
  const auto completedReads = test.Env.Stack.BuildingAssets()->Costs().BasisReadBytes;
  const auto replay = test.Env.Stack.BuildingAssets()->LoadBasisRequest(test.Request);
  const auto after = test.Env.Stack.BuildingAssets()->Costs();
  CHECK(replay && *replay && (**replay).Product == product &&
            after.BasisReadBytes == completedReads && after.BasisResidentHits > 0,
        "the bound request reuses the retained basis owner without another blob decode");
  Fixture fresh;
  CHECK(fresh.Open(test.Env.Directory.Path, true), "a fresh owner opens the committed cache");
  if (!fresh.Stack.BuildingAssets()) { return; }
  const auto restored = fresh.Stack.BuildingAssets()->LoadBasisRequest(test.Request);
  CHECK(restored && *restored && (**restored).BaseKey == test.Key &&
            (**restored).Bounds == test.Bounds,
        "the resumed alias and exact source bounds survive fresh native ownership");
}

void Cancel() {
  Case test;
  if (!test.Open() || !test.WaitHeld()) { return; }
  test.Env.Queue.Clear();
  test.ReleaseWriter();
  const auto restored = test.Env.Stack.BuildingAssets()->LoadBasisRequest(test.Request);
  CHECK(restored && !*restored && test.Env.Queue.Queued() == 0,
        "clearing the actual queue cancels deferred basis work without a later commit");
  auto cache = AssetCache::Open((test.Env.Directory.Path / "assets/assets.sqlite").string());
  const auto base = cache ? (*cache)->Load(test.Key, size_t{64} * 1024 * 1024)
                          : std::expected<std::optional<CachedAsset>, AssetCacheError>{
                                std::unexpected(AssetCacheError::Storage)};
  CHECK(base && *base &&
            Generators::DecodePreparedStructureIndex((**base).Bytes(), size_t{128} * 1024 * 1024),
        "cancellation preserves the complete native index without publishing the missing basis");
}
}

int main() {
  Resume();
  Cancel();
  return Report();
}
