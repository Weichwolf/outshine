#include "BuildingAssetFixture.h"
#include "AssetCache.h"
#include "PreparedBuildingAssets.h"
#include "PreparedBuildingBasis.h"
#include "PreparedStructureTile.h"
#include "PreparedStructurePlan.h"
#include "StructureBuildTask.h"
#include "StructureArtifact.h"
#include "BuildingMesh.h"
#include "SourceSet.h"
#include "RawSourceCache.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
using namespace outshine::Test::BuildingAssetFixture;
constexpr std::string_view kInputDigest = "basis-miss-revalidation-native-inputs";
constexpr auto kDeadline = std::chrono::seconds(5);

std::string Key(const PreparedBuildingAssets &cache) {
  return cache.Key({.Zoom = 0, .X = 0, .Y = 0}, 17, {}, 1000, kInputDigest);
}

std::string Request(const PreparedBuildingAssets &cache) {
  return cache.RequestKey({.Zoom = 0, .X = 0, .Y = 0}, {}, 1000, kInputDigest);
}

bool Seed(PreparedBuildingAssets &cache) {
  const auto heights = Heights();
  const std::atomic_bool stopping{false};
  const auto generated = cache.Generate(Key(cache), Inputs(), *heights, stopping, Request(cache));
  return generated && *generated;
}

struct Fixture {
  std::filesystem::path Directory =
      std::filesystem::temp_directory_path() /
      ("outshine-basis-miss-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Data::RawSourceCache Raw{{.Directory = {}, .Using = Data::RawSourceCache::Use::Off}};
  const Data::SourceSet Sources{Raw};
  Tasks Pool{1};
  BuildingMesh Mesher;
  std::shared_ptr<PreparedBuildingAssets> Cache;
  std::shared_ptr<const PreparedBuildingBasis> Basis;
  std::unique_ptr<AssetCache> Records;
  std::unique_ptr<StructureBuildTask> Job;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};

  ~Fixture() {
    if (Job) {
      Job->RequestStop();
      Job->Join(Pool);
    }
    Job.reset();
    Writer.reset();
    Records.reset();
    Basis.reset();
    Cache.reset();
    std::error_code error;
    std::filesystem::remove_all(Directory, error);
    CHECK(!error, "private products are removed after native tasks, leases and cache owners end");
  }

  bool Open() {
    auto cache = PreparedBuildingAssets::Open(Directory.string(), Sources);
    if (!cache || !Seed(**cache)) { return false; }
    Cache = std::move(*cache);
    auto basis = Cache->LoadBasis(Key(*Cache), Request(*Cache));
    if (!basis || !*basis) { return false; }
    Basis = std::move(*basis);
    auto records = AssetCache::Open((Directory / "assets.sqlite").string());
    if (!records) { return false; }
    Records = std::move(*records);
    if (!Records->Remove(Key(*Cache))) { return false; }
    sqlite3 *connection = nullptr;
    const int status = sqlite3_open((Directory / "assets.sqlite").c_str(), &connection);
    Writer.reset(connection);
    return status == SQLITE_OK;
  }

  void Start(std::optional<LevelOfDetail> detail = std::nullopt) {
    auto view = std::make_unique<RawTile>();
    view->Eye = {.LongitudeDeg = 9.001, .LatitudeDeg = 47.001};
    view->Projection = {.FocalPx = 720};
    view->RequestedDetail = detail;
    Job = std::make_unique<StructureBuildTask>(0,
                                               Basis,
                                               std::move(view),
                                               std::make_unique<StructureBuildTask::Output>(),
                                               Mesher.Scratch());
    Job->UseNativeAssets(Cache, Key(*Cache), Request(*Cache));
    Job->Start(Pool, Mesher);
    Job->Join(Pool);
  }

  void Resume() {
    if (Job->CanResume()) {
      Job->Resume(Pool, Mesher);
      Job->Join(Pool);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  bool Await() {
    const auto until = std::chrono::steady_clock::now() + kDeadline;
    while (Job->Result().Status && !Job->Result().Tile &&
           std::chrono::steady_clock::now() < until) {
      Resume();
    }
    return Job->Result().Status && Job->Result().Tile.has_value();
  }
};

void CheckBusyShapeCell(std::optional<LevelOfDetail> detail, bool cancel) {
  Fixture fixture;
  const bool opened = fixture.Open() && Seed(*fixture.Cache);
  CHECK(opened, "the lazy-cell case retains its actual native index and basis");
  if (!opened) { return; }
  const auto parent = fixture.Records->Find(Key(*fixture.Cache));
  CHECK(parent && *parent, "native parent bounds identify the shape children");
  if (!parent || !*parent) { return; }
  const auto cells = fixture.Records->Select({.Bounds = (**parent).Bounds,
                                              .Kind = "building-forms",
                                              .Level = {},
                                              .Radius = {},
                                              .Planes = {}});
  CHECK(cells && cells->size() == 1, "the fixture has exactly one occupied native form cell");
  if (!cells || cells->size() != 1) { return; }
  CHECK(fixture.Records->Remove(cells->front().Key), "only its regenerable form cell is missing");
  const auto before = fixture.Cache->Costs();
  CHECK(sqlite3_exec(fixture.Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "another real writer prevents publication of that missing cell");
  fixture.Start(detail);
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
  while (std::chrono::steady_clock::now() < until && fixture.Job->Result().Status) {
    fixture.Resume();
  }
  const auto waiting = fixture.Cache->Costs();
  CHECK(fixture.Job->Result().Status && !fixture.Job->Result().Tile &&
            waiting.SurfacePreparations == before.SurfacePreparations + 1 &&
            waiting.SurfaceMisses == before.SurfaceMisses + 1 &&
            waiting.SurfaceWrites == before.SurfaceWrites,
        "the same actual task retains one prepared form product/package across real Busy");
  const auto independent = fixture.Pool.Post([] {});
  CHECK(independent != Tasks::kNoTask && fixture.Pool.AwaitCompletion(independent, 1),
        "cell contention releases the single worker for independent admitted work");
  if (independent != Tasks::kNoTask) { (void)fixture.Pool.TakeCompletion(independent); }
  if (cancel) {
    fixture.Job->RequestStop();
    fixture.Resume();
    const auto &status = fixture.Job->Result().Status;
    const auto *reason = status ? nullptr : std::get_if<StructureBakeErrorKind>(&status.error());
    CHECK(reason && *reason == StructureBakeErrorKind::Cancelled,
          "cancellation retires the blocked task without retrying publication");
    fixture.Job.reset();
    CHECK(sqlite3_exec(fixture.Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the writer releases after task cancellation");
    const auto absent = fixture.Records->Find(cells->front().Key);
    CHECK(absent && !*absent && fixture.Cache->Costs().SurfaceWrites == before.SurfaceWrites,
          "a cancelled demand does not publish the retained cell later");
    return;
  }
  CHECK(sqlite3_exec(fixture.Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the writer releases native cell publication");
  CHECK(fixture.Await(), "the same task resumes its retained geometry progress and publication");
  const auto &tile = fixture.Job->Result().Tile;
  if (!tile) { return; }
  const auto heights = Heights();
  const auto original = PrepareStructureTile(Inputs(), *heights);
  CHECK(original, "the unchanged input yields an independent complete geometry reference");
  if (!original) { return; }
  RawTile view;
  view.Eye = {.LongitudeDeg = 9.001, .LatitudeDeg = 47.001};
  view.Projection = {.FocalPx = 720};
  view.RequestedDetail = detail;
  auto scratch = fixture.Mesher.Scratch();
  const auto expected = BakePreparedStructures(*original, view, fixture.Mesher, *scratch);
  const auto &actual = *tile;
  CHECK(expected && actual.Digest == expected->Digest && actual.Prints == expected->Prints &&
            actual.Prints.size() == original->Structures.size() &&
            fixture.Cache->Costs().SurfacePreparations == waiting.SurfacePreparations &&
            fixture.Cache->Costs().SurfaceWrites == before.SurfaceWrites + 1,
        "resumption preserves geometry/contacts without duplicate metadata or cell preparation");
}
}

int main() {
  CheckBusyShapeCell(LevelOfDetail::Shell, false);
  CheckBusyShapeCell(std::nullopt, false);
  CheckBusyShapeCell(LevelOfDetail::Shell, true);
  return outshine::Test::Report();
}
