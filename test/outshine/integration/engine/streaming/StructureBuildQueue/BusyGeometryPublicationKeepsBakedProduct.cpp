#include "test/harness/engine/NativeBuildingSources.h"
#include "AssetCache.h"
#include "BuildingMesh.h"
#include "Check.h"
#include "PreparedBuildingAssets.h"
#include "StructureArtifact.h"
#include "StructureBuildTask.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <thread>
#include <utility>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
using outshine::Test::BuildingSources::Fixture;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr auto kWait = std::chrono::milliseconds(1);
using Writer = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

class CountingMesh final : public BuildingMesh {
public:
  mutable size_t Calls = 0;

  std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &scratch, Raised &into) const noexcept override {
    ++Calls;
    return BuildingMesh::Mesh(plan, scratch, into);
  }
};

struct Case {
  CountingMesh Mesher;
  Fixture Env;
  std::unique_ptr<StructureBuildTask> Work;
  Writer Database{nullptr, sqlite3_close};
  std::string Key;

  ~Case() {
    if (Work) {
      Work->RequestStop();
      Work->Join(Env.Worker);
    }
  }

  bool Open() {
    CHECK(Env.Open(), "the real native source/cache fixture opens");
    if (!Env.Stack.BuildingAssets()) { return false; }
    const auto generated = Env.Generate();
    CHECK(generated, "source products are complete before geometry publication contends");
    if (!generated) { return false; }
    Key = *generated;
    const auto base = Env.Stack.BuildingAssets()->Load(Key);
    CHECK(base && *base, "the actual prepared native index is resident");
    if (!base || !*base) { return false; }
    auto view = std::make_unique<RawTile>();
    view->Eye = Env.Eye;
    view->Projection = {.FocalPx = 720};
    Work = std::make_unique<StructureBuildTask>(0,
                                                *base,
                                                std::move(view),
                                                std::make_unique<StructureBuildTask::Output>(),
                                                Mesher.Scratch());
    Work->UseNativeAssets(Env.Stack.BuildingAssets(), Key);
    Work->UseInitialCells();
    sqlite3 *raw = nullptr;
    const auto path = Env.Directory.Path / "assets" / "assets.sqlite";
    const int opened = sqlite3_open(path.c_str(), &raw);
    Database.reset(raw);
    CHECK(opened == SQLITE_OK, "an independent writer opens the real asset database");
    if (opened != SQLITE_OK) { return false; }
    CHECK(sqlite3_exec(raw, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "geometry publication faces actual external writer admission");
    return true;
  }

  void Pump() {
    if (Work->CanResume()) { Work->Resume(Env.Worker, Mesher); }
    Work->Join(Env.Worker);
    std::this_thread::sleep_for(kWait);
  }

  bool WaitHeld() {
    Work->Start(Env.Worker, Mesher);
    Work->Join(Env.Worker);
    const auto until = std::chrono::steady_clock::now() + kDeadline;
    while (std::chrono::steady_clock::now() < until) {
      CHECK(Work->Result().Status && !Work->Result().Tile,
            "valid baked geometry stays unpublished while its cache commit is busy");
      if (!Work->Result().Status || Work->Result().Tile) { return false; }
      if (Work->Result().PublicationBytes > 0) { return true; }
      Pump();
    }
    CHECK(false, "bounded native meshing reaches a real retained geometry publication");
    return false;
  }

  void ReleaseWriter() const {
    CHECK(sqlite3_exec(Database.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases geometry publication admission");
  }
};

void Resume() {
  Case test;
  if (!test.Open() || !test.WaitHeld()) { return; }
  const auto calls = test.Mesher.Calls;
  const auto bytes = test.Work->Result().PublicationBytes;
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
  while (std::chrono::steady_clock::now() < until) {
    test.Pump();
    CHECK(test.Work->Result().Status && !test.Work->Result().Tile &&
              test.Work->Result().PublicationBytes == bytes,
          "only admission is retried; the complete package and baked product remain owned");
  }
  const auto held = test.Env.Stack.BuildingAssets()->Costs();
  CHECK(calls > 0 && test.Mesher.Calls == calls && held.GeometryPreparations == 1 &&
            held.GeometryWrites == 0 && test.Work->Result().PublicationAttempts > 1 &&
            test.Work->Result().PublicationAttempts < 20,
        "bounded retries neither remesh nor encode/compress another package");
  bool progressed = false;
  const auto other = test.Env.Worker.Post([&progressed] { progressed = true; });
  CHECK(other != Tasks::kNoTask, "unrelated work enters the existing worker pool");
  if (other != Tasks::kNoTask) { test.Env.Worker.Wait(other); }
  CHECK(progressed, "writer contention does not occupy a sleeping or spinning worker");
  test.ReleaseWriter();
  const auto resumed = std::chrono::steady_clock::now() + kDeadline;
  while (!test.Work->Result().Tile && test.Work->Result().Status &&
         std::chrono::steady_clock::now() < resumed) {
    test.Pump();
  }
  const auto &tile = test.Work->Result().Tile;
  CHECK(test.Work->Result().Status && tile && tile->GeometryAssetKey.size() == 64 &&
            test.Work->Result().PublicationBytes == 0 && test.Mesher.Calls == calls,
        "successful commit exposes the retained complete geometry and releases its package");
  if (!tile) { return; }
  const auto encoded = EncodeStructureArtifact(*tile, tile->GeometryAssetKey);
  auto cache = AssetCache::Open((test.Env.Directory.Path / "assets/assets.sqlite").string());
  CHECK(cache && encoded, "the resumed native payload is independently verifiable");
  if (!cache || !encoded) { return; }
  const auto restored = (*cache)->Load(tile->GeometryAssetKey, size_t{64} * 1024 * 1024);
  CHECK(restored && *restored && std::ranges::equal((**restored).Bytes(), *encoded),
        "stored bytes exactly preserve the complete baked artifact after writer contention");
}

void Cancel() {
  Case test;
  if (!test.Open() || !test.WaitHeld()) { return; }
  test.Work->RequestStop();
  test.Work->Resume(test.Env.Worker, test.Mesher);
  test.Work->Join(test.Env.Worker);
  CHECK(!test.Work->Result().Status && !test.Work->Result().Tile &&
            test.Work->Result().PublicationBytes == 0,
        "cancellation drops both pending geometry owners without returning a visible product");
  test.ReleaseWriter();
  CHECK(test.Env.Stack.BuildingAssets()->Costs().GeometryWrites == 0,
        "cancelled native geometry never commits after writer admission becomes available");
}

void QueueResume() {
  Case test;
  if (!test.Open()) { return; }
  test.Env.Queue.Opens(&test.Env.Worker, &test.Mesher);
  auto &prints = test.Env.Stack.Footprints();
  prints.SeenWith({.FocalPx = 720});
  const StructureBuildQueue::HeightSource source{
      .Sample = {}, .PinField = {}, .Revision = {.Value = 11}};
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (std::chrono::steady_clock::now() < until) {
    (void)test.Env.Queue.Posts(test.Env.Stack, prints, test.Env.Eye, source, 1);
    const auto landed =
        test.Env.Queue.NextLandings(test.Env.Stack, prints, test.Env.Eye, source, 1);
    CHECK(landed && landed->empty(), "the real queue exposes no landing before geometry commit");
    if (!landed) { return; }
    if (test.Env.Stack.BuildingAssets()->Costs().GeometryPreparations > 0) { break; }
    std::this_thread::sleep_for(kWait);
  }
  CHECK(test.Env.Stack.BuildingAssets()->Costs().GeometryPreparations == 1,
        "the actual queue reaches the prepared pending geometry");
  const auto barrier = test.Env.Worker.Post([] {});
  CHECK(barrier != Tasks::kNoTask, "the existing worker exposes completion before cost capture");
  if (barrier != Tasks::kNoTask) { test.Env.Worker.Wait(barrier); }
  const auto calls = test.Mesher.Calls;
  test.ReleaseWriter();
  const auto resumed = std::chrono::steady_clock::now() + kDeadline;
  bool complete = false;
  while (std::chrono::steady_clock::now() < resumed) {
    const auto landed =
        test.Env.Queue.NextLandings(test.Env.Stack, prints, test.Env.Eye, source, 1);
    CHECK(landed, "the existing queue resumes its completed task after the retry deadline");
    if (!landed) { return; }
    if (!landed->empty()) {
      complete =
          landed->front().Baked != nullptr && landed->front().Baked->GeometryAssetKey.size() == 64;
      break;
    }
    std::this_thread::sleep_for(kWait);
  }
  CHECK(complete && test.Mesher.Calls == calls &&
            test.Env.Stack.BuildingAssets()->Costs().GeometryWrites == 1,
        "the real queue lands the retained native geometry exactly once without another mesh");
  test.Env.Queue.Clear();
}

void RejectPermanentFailure() {
  Case test;
  if (!test.Open()) { return; }
  test.ReleaseWriter();
  CHECK(sqlite3_exec(test.Database.get(),
                     "CREATE TRIGGER reject_geometry BEFORE INSERT ON assets "
                     "WHEN NEW.kind IN ('building-lod','building-static-lod') "
                     "BEGIN SELECT RAISE(ABORT, 'fixture storage failure'); END",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "a permanent constraint rejects only the native geometry commit");
  test.Work->Start(test.Env.Worker, test.Mesher);
  test.Work->Join(test.Env.Worker);
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (test.Work->Result().Status && !test.Work->Result().Tile &&
         std::chrono::steady_clock::now() < until) {
    test.Pump();
  }
  CHECK(!test.Work->Result().Status && !test.Work->Result().Tile &&
            test.Work->Result().PublicationBytes == 0 &&
            test.Work->Result().PublicationAttempts == 1,
        "permanent storage failure releases retained data and never becomes a busy retry");
}
}

int main() {
  Resume();
  Cancel();
  QueueResume();
  RejectPermanentFailure();
  return Report();
}
