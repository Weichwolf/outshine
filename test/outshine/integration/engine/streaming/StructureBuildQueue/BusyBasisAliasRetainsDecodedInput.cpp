#include "test/harness/engine/NativeBuildingSources.h"
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
  uint64_t Digest = 0;
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
    Digest = *digest;
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
    sqlite3_stmt *statement = nullptr;
    const int prepared = sqlite3_prepare_v2(
        raw, "DELETE FROM asset_requests WHERE request_key=?", -1, &statement, nullptr);
    CHECK(prepared == SQLITE_OK, "only the private request alias is selected for removal");
    if (prepared != SQLITE_OK) { return false; }
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> erase(statement,
                                                                           sqlite3_finalize);
    CHECK(sqlite3_bind_text(statement, 1, Request.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK &&
              sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(raw) == 1,
          "the native basis and sources survive removal of exactly one alias");
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
      if (Env.Stack.BuildingAssets()->Costs().BasisReadBytes > Before.BasisReadBytes) {
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
  CHECK(waiting.BasisReadBytes == held.BasisReadBytes &&
            waiting.BasisWrites == test.Before.BasisWrites &&
            waiting.Writes == test.Before.Writes && test.Env.Queue.Queued() <= 4,
        "retry retains the decoded basis without repeated decode, generation or extra slots");
  test.Env.Queue.PollCompletedTasks();
  const auto began = std::chrono::steady_clock::now();
  (void)test.Env.Queue.AwaitSlice(0.5);
  const auto waited = std::chrono::steady_clock::now() - began;
  CHECK(waited < std::chrono::milliseconds(250),
        "the existing slice wait respects the pending retry deadline instead of sleeping the full "
        "slice");
  const auto independent = std::make_shared<std::atomic_bool>(false);
  const auto job = test.Env.Worker.Post([independent] { independent->store(true); });
  const bool finished = job != Tasks::kNoTask && test.Env.Worker.AwaitCompletion(job, 0.25);
  CHECK(finished && independent->load(),
        "an unrelated task runs on the single worker while alias publication is deferred");
  if (finished) { test.Env.Worker.Wait(job); }
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
  const auto replay = test.Env.Stack.BuildingAssets()->LoadBasisRequest(test.Request);
  const auto after = test.Env.Stack.BuildingAssets()->Costs();
  CHECK(replay && *replay && (**replay).Product == product &&
            after.BasisReadBytes == held.BasisReadBytes && after.BasisResidentHits > 0,
        "the bound request reuses the retained basis owner without another blob decode");
  Fixture fresh;
  CHECK(fresh.Open(test.Env.Directory.Path, true), "a fresh owner opens the committed cache");
  if (!fresh.Stack.BuildingAssets()) { return; }
  const auto restored = fresh.Stack.BuildingAssets()->LoadBasisRequest(test.Request);
  CHECK(restored && *restored && (**restored).BaseKey == test.Key &&
            (**restored).Bounds == test.Bounds,
        "the resumed alias and exact source bounds survive fresh native ownership");
}

void Rebound() {
  Case test;
  if (!test.Open() || !test.WaitHeld()) { return; }
  test.ReleaseWriter();
  const auto replacement = test.Env.Generate(test.Digest + 1);
  CHECK(replacement && *replacement != test.Key,
        "another native owner publishes a newer valid target before the held alias resumes");
  if (!replacement) { return; }
  std::optional<StructureBuildQueue::NativeBasisQuery> ready;
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto query =
        test.Env.Queue.PollNativeBasis(test.Env.Stack, test.Env.Stack.Footprints(), 0);
    CHECK(query, "a resumed alias can observe a concurrent valid target");
    if (!query) { return; }
    if (query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Ready) {
      ready = *query;
      break;
    }
    (void)test.Env.Worker.AwaitCompletion(0.001);
  }
  const auto durable = test.Env.Stack.BuildingAssets()->LoadBasisRequest(test.Request);
  CHECK(ready && ready->Source && ready->Source->BaseKey == *replacement && durable && *durable &&
            (**durable).BaseKey == *replacement,
        "old retained work neither overwrites nor returns the superseded request target");
}

void Cancel() {
  Case test;
  if (!test.Open() || !test.WaitHeld()) { return; }
  test.Env.Queue.Clear();
  test.ReleaseWriter();
  const auto restored = test.Env.Stack.BuildingAssets()->LoadBasisRequest(test.Request);
  CHECK(restored && !*restored && test.Env.Queue.Queued() == 0,
        "clearing the actual queue cancels deferred alias work without a later commit");
  const auto basis = test.Env.Stack.BuildingAssets()->LoadBasis(test.Key);
  CHECK(basis && *basis, "cancellation preserves the original complete native basis");
}
}

int main() {
  Resume();
  Cancel();
  Rebound();
  return Report();
}
