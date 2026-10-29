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
            after.BasisPreparations == before.BasisPreparations,
        "rejecting a wrong-kind alias preserves its valid target and all existing native work");
}

void BusyRetirementAndRepair() {
  Case test;
  if (!test.Open()) { return; }
  auto &native = *test.Env.Stack.BuildingAssets();
  sqlite3 *raw = nullptr;
  const int opened = sqlite3_open((test.Env.Directory.Path / "assets/assets.sqlite").c_str(), &raw);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> writer(raw, sqlite3_close);
  CHECK(opened == SQLITE_OK, "an independent native writer opens alias retirement admission");
  if (opened != SQLITE_OK) { return; }
  CHECK(sqlite3_exec(raw, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the independent writer holds alias retirement");
  Generators::PreparedBuildingAssets::BasisRead read;
  std::atomic_bool stopping{false};
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    const auto held = native.ReadBasisRequest(test.Request, read, stopping);
    const auto *kind =
        held ? nullptr : std::get_if<Generators::StructureBakeErrorKind>(&held.error());
    CHECK(kind && *kind == Generators::StructureBakeErrorKind::ArtifactBusy && read.Pending(),
          "invalid binding retirement remains owned and pending during writer contention");
  }
  stopping.store(true, std::memory_order_relaxed);
  const auto cancelled = native.ReadBasisRequest(test.Request, read, stopping);
  const auto *kind =
      cancelled ? nullptr : std::get_if<Generators::StructureBakeErrorKind>(&cancelled.error());
  const auto retained = test.Cache->FindRequest(test.Request);
  CHECK(kind && *kind == Generators::StructureBakeErrorKind::Cancelled && !read.Pending() &&
            retained && *retained && (**retained).Key == test.Key,
        "cancellation releases retirement work without deleting its alias or valid target");
  stopping.store(false, std::memory_order_relaxed);
  const auto again = native.ReadBasisRequest(test.Request, read, stopping);
  CHECK(!again && read.Pending(),
        "another admitted caller can retry the unchanged invalid binding");
  CHECK(sqlite3_exec(raw, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the independent writer releases retirement admission");
  const auto replacement = test.Env.Generate(test.Digest + 1);
  CHECK(replacement && *replacement != test.Key,
        "another native owner publishes a different valid basis and repairs the same request");
  if (!replacement) { return; }
  const auto repaired = native.ReadBasisRequest(test.Request, read, stopping);
  CHECK(repaired && *repaired && (**repaired).BaseKey == *replacement && !read.Pending(),
        "a resumed retirement observes the newer valid binding without removing or overwriting it");
}

}

int main() {
  InvalidAlias();
  BusyRetirementAndRepair();
  return Report();
}
