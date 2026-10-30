#include "AssetCache.h"
#include "Check.h"
#include "TerrainDeformationTask.h"
#include "PreparedTerrainAssets.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <thread>
#include <variant>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr TerrainPageLayout kLayout{.Side = 3, .Halo = 1};

Patchwork Inputs() {
  Patchwork pages;
  for (uint32_t column = 0; column < 4; ++column) {
    pages.Sheets.push_back({.Tile = {.Zoom = 20, .X = (1u << 19u) + column, .Y = 1u << 19u},
                            .Nodes = std::vector<float>(kLayout.NodeCount(), 1.0f),
                            .Side = kLayout.Side,
                            .Postings = 3,
                            .Virtual = true,
                            .SourceZoom = 17});
  }
  pages.Tiles = 4;
  pages.ReachTiles = 4;
  return pages;
}

std::vector<EarthworkStamp> Stamps() {
  EarthworkStamp stamp{.RingEastNorthM = {-100, -100, 200, -100, 200, 100, -100, 100},
                       .HoleRingsEastNorthM = {},
                       .LowE = -100,
                       .HighE = 200,
                       .LowN = -100,
                       .HighN = 100,
                       .PlateauM = 7,
                       .SeamEastNorthM = {},
                       .Profile = {},
                       .PlanarSupportM = {},
                       .ApronM = 3,
                       .Fills = true,
                       .Kind = EarthworkKind::Pad};
  stamp.HoleRingsEastNorthM.push_back({0, 0, 5, 0, 5, 5, 0, 5});
  return {stamp};
}

struct Fixture {
  std::filesystem::path Directory =
      std::filesystem::temp_directory_path() /
      ("outshine-deformation-busy-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Data::RawSourceCache Raw{{.Directory = {}, .Using = Data::RawSourceCache::Use::Off}};
  const Data::SourceSet Sources{Raw};
  Tasks Pool{1};
  std::shared_ptr<PreparedTerrainAssets> Cache;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};

  Fixture() {
    auto cache = PreparedTerrainAssets::Open(Directory.string(), Sources);
    CHECK(cache.has_value(), "the case opens a private native cache without source access");
    if (!cache) { return; }
    Cache = std::move(*cache);
    sqlite3 *database = nullptr;
    const auto opened = sqlite3_open((Directory / "assets.sqlite").c_str(), &database);
    Writer.reset(database);
    CHECK(opened == SQLITE_OK, "a separate SQLite connection controls real writer admission");
  }

  ~Fixture() {
    Writer.reset();
    Cache.reset();
    std::error_code error;
    std::filesystem::remove_all(Directory, error);
    CHECK(!error, "private native inputs and products are removed after their owners end");
  }

  bool Hold() const {
    return Writer &&
           sqlite3_exec(Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK;
  }

  void Release() const {
    CHECK(sqlite3_exec(Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases admission");
  }

  void Drain() {
    const auto task = Pool.Post([] {});
    CHECK(task != Tasks::kNoTask && Pool.AwaitCompletion(task, 1),
          "an unrelated task uses the same single worker while cache publication is deferred");
    if (task != Tasks::kNoTask) { (void)Pool.TakeCompletion(task); }
  }

  bool AwaitPublication(TerrainDeformationTask &task) const {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (Cache->Costs().DeformationPublicationAttempts == 0) {
      if (task.Advance() || std::chrono::steady_clock::now() >= deadline) { return false; }
      (void)task.AwaitSlice(0.001);
    }
    return true;
  }
};

std::expected<TerrainDeformationMetrics, std::string> Complete(TerrainDeformationTask &task,
                                                               Patchwork &pages) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!task.Advance()) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return std::unexpected("deformation deadline");
    }
    (void)task.AwaitSlice(0.001);
  }
  return task.Take(pages);
}

void ColdAndWarm() {
  Fixture fixture;
  if (!fixture.Cache || !fixture.Writer) { return; }
  const auto frame = TangentFrame::At({});
  auto reference = Inputs();
  TerrainDeformationJob original(Stamps(), reference, frame, kLayout, 30);
  while (!original.Advance(32, 8192)) {}
  const auto expected = original.Take();
  CHECK(expected.Nodes > 0,
        "the case deforms real native heights rather than publishing an empty product");
  CHECK(fixture.Hold(), "a real writer blocks cold native publication");
  auto cold = Inputs();
  TerrainDeformationTask task(fixture.Pool, fixture.Cache, Stamps(), cold, frame, kLayout, 30);
  CHECK(cold.Sheets.empty(),
        "the admitted task owns the native pages without copying caller storage");
  CHECK(fixture.AwaitPublication(task),
        "generation reaches a real deferred native publication attempt");
  fixture.Drain();
  const auto before = fixture.Cache->Costs();
  const auto heap = task.HeapBytes();
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  const auto idle = fixture.Cache->Costs();
  CHECK(before.DeformationPreparations == 1 && before.DeformationPublicationAttempts >= 1 &&
            idle.DeformationPreparations == before.DeformationPreparations &&
            idle.DeformationPublicationAttempts == before.DeformationPublicationAttempts,
        "a deferred task neither reencodes its native package nor spins without caller admission");
  CHECK(heap > reference.HeapBytes() && task.HeapBytes() == heap,
        "owned native page and prepared-package capacities remain visible throughout deferral");
  CHECK(!task.Advance(), "typed Busy keeps the same native task pending");
  fixture.Release();
  const auto result = Complete(task, cold);
  CHECK(result && result->Nodes == expected.Nodes && cold.Sheets == reference.Sheets,
        "the admitted task resumes the exact deformed result after writer release");
  const auto published = fixture.Cache->Costs();
  CHECK(published.DeformationMisses == 1 && published.DeformationPreparations == 1 &&
            published.DeformationWrites == 1 && published.DeformationPublicationAttempts >= 2,
        "Busy repeats only admission of one prepared package, not generation or encoding");
  auto reopened = PreparedTerrainAssets::Open(fixture.Directory.string(), fixture.Sources);
  CHECK(reopened.has_value(), "a fresh native cache connection reads the durable product");
  if (!reopened) { return; }
  CHECK(fixture.Hold(), "warm native reads run while another writer is held");
  auto warm = Inputs();
  TerrainDeformationTask replay(fixture.Pool, *reopened, Stamps(), warm, frame, kLayout, 30);
  const auto hit = Complete(replay, warm);
  CHECK(hit && hit->Nodes == expected.Nodes && hit->GatherMs == 0 && hit->ApplyMs == 0 &&
            warm.Sheets == reference.Sheets,
        "warm replay under a writer uses native pages and preserves every deformed height");
  CHECK((*reopened)->Costs().DeformationHits == 1 &&
            (*reopened)->Costs().DeformationPreparations == 0 &&
            (*reopened)->Costs().DeformationPublicationAttempts == 0,
        "a valid native hit never asks for writer admission or regenerates a package");
  fixture.Release();
}

void InvalidRead() {
  Fixture fixture;
  if (!fixture.Cache || !fixture.Writer) { return; }
  const auto key = TerrainDeformationKey(Inputs(), Stamps(), TangentFrame::At({}), kLayout, 30);
  CHECK(key.has_value(), "the invalid-read case has the actual native request identity");
  if (!key) { return; }
  auto storage = AssetCache::Open((fixture.Directory / "assets.sqlite").string());
  CHECK(storage.has_value(), "the case opens native storage for an invalid CRC-correct product");
  if (!storage) { return; }
  const std::array<uint8_t, 4> bytes{1, 2, 3, 4};
  const AssetRecord record{.Key = *key,
                           .Kind = "terrain-deformed",
                           .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                           .Package = {},
                           .ByteCount = bytes.size(),
                           .Parent = {}};
  CHECK((*storage)->Publish({&record, 1}, bytes).has_value(),
        "the native package has invalid product bytes");
  CHECK(fixture.Hold(), "the writer prevents retirement of the invalid native root");
  auto input = Inputs();
  TerrainDeformationTask task(
      fixture.Pool, fixture.Cache, Stamps(), input, TangentFrame::At({}), kLayout, 30);
  fixture.Drain();
  CHECK(!task.Advance() && fixture.Cache->Costs().DeformationPreparations == 0,
        "typed Busy from native invalidation defers lookup before generation");
  const auto retained = (*storage)->Find(*key);
  CHECK(retained && *retained, "failed retirement admission preserves the current root");
  fixture.Release();
  const auto result = Complete(task, input);
  CHECK(result && result->Nodes > 0 && fixture.Cache->Costs().DeformationWrites == 1,
        "the same lookup task retires the bad root and generates once after admission");
}

void CancelledPublication() {
  Fixture fixture;
  if (!fixture.Cache || !fixture.Writer) { return; }
  CHECK(fixture.Hold(), "cancellation starts with native writer admission blocked");
  auto input = Inputs();
  const auto key = TerrainDeformationKey(input, Stamps(), TangentFrame::At({}), kLayout, 30);
  auto task = std::make_unique<TerrainDeformationTask>(
      fixture.Pool, fixture.Cache, Stamps(), input, TangentFrame::At({}), kLayout, 30);
  CHECK(fixture.AwaitPublication(*task),
        "the cancelled task owns a prepared unpublished native product");
  fixture.Drain();
  const auto attempts = fixture.Cache->Costs().DeformationPublicationAttempts;
  const auto started = std::chrono::steady_clock::now();
  task.reset();
  CHECK(std::chrono::steady_clock::now() - started < std::chrono::milliseconds(100),
        "cancellation does not wait for writer admission");
  fixture.Release();
  fixture.Drain();
  const auto absent = key ? fixture.Cache->LoadDeformation(*key)
                          : std::expected<std::optional<PreparedTerrainDeformation>,
                                          PreparedTerrainAssets::Failure>{
                                std::unexpected(AssetCacheError::InvalidInput)};
  CHECK(absent && !*absent && fixture.Cache->Costs().DeformationWrites == 0 &&
            fixture.Cache->Costs().DeformationPublicationAttempts == attempts,
        "no deferred publication runs after its owner is cancelled and the writer is released");
}

void PermanentStorageFailure() {
  Fixture fixture;
  if (!fixture.Cache || !fixture.Writer) { return; }
  CHECK(sqlite3_exec(fixture.Writer.get(),
                     "CREATE TRIGGER deny_deformation BEFORE INSERT ON assets "
                     "BEGIN SELECT RAISE(ABORT,'native storage denied'); END",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "a real database trigger rejects native publication permanently");
  auto input = Inputs();
  const auto frame = TangentFrame::At({});
  const auto key = TerrainDeformationKey(input, Stamps(), frame, kLayout, 30);
  CHECK(key.has_value(), "the failed publication retains its actual request identity");
  if (!key) { return; }
  TerrainDeformationTask task(fixture.Pool, fixture.Cache, Stamps(), input, frame, kLayout, 30);
  const auto result = Complete(task, input);
  CHECK(!result && result.error() == "terrain asset cache storage failed",
        "permanent storage failure ends the task explicitly instead of deferring it");
  fixture.Drain();
  const auto costs = fixture.Cache->Costs();
  CHECK(task.Advance() && costs.DeformationPreparations == 1 &&
            costs.DeformationPublicationAttempts == 1 && costs.DeformationWrites == 0,
        "a failed permanent publication never restarts preparation or writer admission");
  const auto absent = fixture.Cache->LoadDeformation(*key);
  CHECK(absent && !*absent, "the rejected publication leaves no visible partial native product");
}

void AwaitFollowsRetryDeadline() {
  Fixture fixture;
  if (!fixture.Cache || !fixture.Writer) { return; }
  CHECK(fixture.Hold(), "a real writer keeps the native retry deferred");
  auto input = Inputs();
  TerrainDeformationTask task(
      fixture.Pool, fixture.Cache, Stamps(), input, TangentFrame::At({}), kLayout, 30);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (fixture.Cache->Costs().DeformationPublicationAttempts < 6 &&
         std::chrono::steady_clock::now() < deadline) {
    (void)task.AwaitSlice(0.001);
  }
  CHECK(fixture.Cache->Costs().DeformationPublicationAttempts >= 6,
        "the real retry reaches its bounded maximum backoff");
  fixture.Drain();
  const auto began = std::chrono::steady_clock::now();
  const bool complete = task.AwaitSlice(0.5);
  CHECK(!complete && std::chrono::steady_clock::now() - began < std::chrono::milliseconds(250),
        "a deferred retry yields near its deadline rather than idling through the entire slice");
  fixture.Release();
  const auto result = Complete(task, input);
  CHECK(result && result->Nodes > 0, "the same task completes normally after the writer releases");
}
}

int main() {
  ColdAndWarm();
  InvalidRead();
  CancelledPublication();
  PermanentStorageFailure();
  AwaitFollowsRetryDeadline();
  return outshine::Test::Report();
}
