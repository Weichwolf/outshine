#include "test/harness/engine/NativeBuildingSources.h"
#include "AssetCache.h"
#include "BuildingMesh.h"
#include "Check.h"
#include "PreparedBuildingAssets.h"
#include "StructureBuildQueue.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <variant>
#include <utility>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::BuildingSources;

struct Case {
  Generators::BuildingMesh Mesher;
  Fixture Env;
  std::unique_ptr<AssetCache> Cache;
  std::string Key;
  std::string Request;
  uint64_t Digest = 0;

  bool Open() {
    CHECK(Env.Open(), "the real source, native cache and queue fixture opens");
    const auto &native = Env.Stack.BuildingAssets();
    const auto *vectors = Env.Stack.Vectors();
    if (!native || vectors == nullptr) { return false; }
    Env.Queue.Opens(&Env.Worker, &Mesher);
    Env.Stack.Footprints().SeenWith({.FocalPx = 720});
    const auto digest = Env.Stack.Ways().SourceDigest(*vectors, 0);
    CHECK(digest, "native content uses the actual runtime street digest");
    if (!digest) { return false; }
    const auto key = Env.Generate(*digest);
    CHECK(key, "the unchanged real producer publishes the complete native product");
    if (!key) { return false; }
    const auto &tile = vectors->Tiles().front();
    Request = native->RequestKey(
        {.Zoom = tile.Z, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)},
        Env.Stack.Pool().Shaped(),
        kSpan,
        tile.InputDigest);
    auto cache = AssetCache::Open((Env.Directory.Path / "assets/assets.sqlite").string());
    CHECK(cache && (*cache)->BindRequest(*key, Request),
          "a valid native index is deliberately aliased where a basis is required");
    if (!cache) { return false; }
    Key = *key;
    Digest = *digest;
    Cache = std::move(*cache);
    return true;
  }
};

void InvalidAlias() {
  Case test;
  if (!test.Open()) { return; }
  auto &env = test.Env;
  const auto &native = env.Stack.BuildingAssets();
  const auto before = native->Costs();
  const StructureBuildQueue::HeightSource noHeights{
      .Sample = {}, .PinField = {}, .Revision = {.Value = 11}};
  std::optional<StructureBuildQueue::NativeBasisQuery> completed;
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  while (std::chrono::steady_clock::now() < deadline) {
    (void)env.Queue.Posts(env.Stack, env.Stack.Footprints(), env.Eye, noHeights, 1);
    const auto query = env.Queue.PollNativeBasis(env.Stack, env.Stack.Footprints(), 0);
    if (!query) { break; }
    if (query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Ready) {
      completed = *query;
      break;
    }
    (void)env.Worker.AwaitCompletion(0.001);
  }
  CHECK(completed && completed->Source && completed->Source->Product &&
            completed->Source->BaseKey == test.Key,
        "the actual queue repairs an invalid alias and reaches existing native content without "
        "height regeneration or repeated fallback");
  const auto original = test.Cache->Find(test.Key);
  const auto after = native->Costs();
  CHECK(original && *original && after.Writes == before.Writes &&
            after.BasisWrites == before.BasisWrites,
        "rejecting a wrong-kind alias preserves its valid target and all existing native work");
}

void BusyRetirementAndRepair() {
  Case test;
  if (!test.Open()) { return; }
  sqlite3 *raw = nullptr;
  const int opened = sqlite3_open((test.Env.Directory.Path / "assets/assets.sqlite").c_str(), &raw);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> writer(raw, sqlite3_close);
  CHECK(opened == SQLITE_OK && writer, "an independent writer opens the alias retirement case");
  if (opened != SQLITE_OK || !writer) { return; }
  CHECK(sqlite3_exec(raw, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the independent writer holds alias retirement admission");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
  while (std::chrono::steady_clock::now() < deadline) {
    const auto query =
        test.Env.Queue.PollNativeBasis(test.Env.Stack, test.Env.Stack.Footprints(), 0);
    CHECK(query && query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
          "writer-blocked invalid-alias retirement stays pending in the actual queue");
    if (!query) { return; }
    (void)test.Env.Worker.AwaitCompletion(0.001);
  }
  CHECK(test.Env.Queue.Queued() == 1,
        "a deferred request-phase retirement retains its original bounded queue slot");
  CHECK(sqlite3_exec(raw, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the independent writer releases alias retirement");
  const auto replacement = test.Env.Generate(test.Digest + 1);
  CHECK(replacement, "another native owner repairs the invalid request binding");
  if (!replacement) { return; }
  std::optional<StructureBuildQueue::NativeBasisQuery> ready;
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (std::chrono::steady_clock::now() < until) {
    const auto query =
        test.Env.Queue.PollNativeBasis(test.Env.Stack, test.Env.Stack.Footprints(), 0);
    CHECK(query, "the original retirement observes the repaired request");
    if (!query) { return; }
    if (query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Ready) {
      ready = *query;
      break;
    }
    (void)test.Env.Worker.AwaitCompletion(0.001);
  }
  CHECK(ready && ready->Source && ready->Source->BaseKey == *replacement,
        "resuming invalid-alias retirement preserves the newly repaired valid target");
}
}

int main() {
  InvalidAlias();
  BusyRetirementAndRepair();
  return Report();
}
