#include "BuildingAssetFixture.h"
#include "AssetCache.h"
#include "PreparedBuildingAssets.h"
#include "PreparedBuildingBasis.h"
#include "PreparedStructureCodec.h"
#include "PreparedStructureTile.h"
#include "BuildingMesh.h"
#include "StructureBuildTask.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "Tasks.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
using namespace outshine::Test::BuildingAssetFixture;
using namespace std::chrono_literals;

struct Fixture {
  std::filesystem::path Root =
      std::filesystem::temp_directory_path() /
      ("outshine-native-legacy-migration-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Data::RawSourceCache Raw{{.Directory = {}, .Using = Data::RawSourceCache::Use::Off}};
  Data::SourceSet Sources{Raw};
  std::shared_ptr<PreparedBuildingAssets> Cache;
  PreparedBuildingAssets::Basis Basis;
  PreparedStructureTile Complete;
  std::unique_ptr<AssetCache> Database;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};
  std::string Key;

  ~Fixture() {
    Writer.reset();
    Database.reset();
    Cache.reset();
    std::error_code ignored;
    std::filesystem::remove_all(Root, ignored);
  }

  [[nodiscard]] bool Seed() {
    const auto heights = Heights();
    const std::atomic_bool stopping{false};
    auto opened = PreparedBuildingAssets::Open(Root.string(), Sources);
    CHECK(opened.has_value(), "the native building service opens a private cache");
    if (!opened) { return false; }
    Cache = std::move(*opened);
    Key = Cache->Key({.Zoom = 0, .X = 0, .Y = 0}, 17, {}, 1000, "owned-legacy-fixture-input");
    auto original = PrepareStructureTile(Inputs(), *heights, &stopping);
    auto generated = Cache->Generate(Key, Inputs(), *heights, stopping);
    auto basis = Cache->LoadBasis(Key);
    auto database = AssetCache::Open((Root / "assets.sqlite").string());
    CHECK(original && generated && *generated && basis && *basis && database,
          "a supported enriched native product and independent basis exist before migration");
    if (!original || !generated || !*generated || !basis || !*basis || !database) { return false; }
    Complete = std::move(*original);
    Basis = std::move(*basis);
    Database = std::move(*database);
    const auto record = Database->Find(Key);
    const auto bytes = EncodePreparedStructureTile(Complete);
    CHECK(record && *record && bytes,
          "the complete original native format encodes under its existing key");
    if (!record || !*record || !bytes) { return false; }
    auto retained = **record;
    retained.ByteCount = bytes->size();
    retained.Package.clear();
    const auto published = Database->Publish(std::span(&retained, 1), *bytes);
    CHECK(published.has_value(),
          "the fixture replaces the index with the supported complete native product");
    if (!published) { return false; }
    sqlite3 *writer = nullptr;
    const auto status = sqlite3_open((Root / "assets.sqlite").c_str(), &writer);
    Writer.reset(writer);
    CHECK(status == SQLITE_OK, "a second real connection controls writer admission");
    return status == SQLITE_OK;
  }

  [[nodiscard]] bool Begin() const {
    const bool busy =
        sqlite3_exec(Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK;
    CHECK(busy, "the independent writer prevents migration publication");
    return busy;
  }

  [[nodiscard]] bool Commit() const {
    const bool ready = sqlite3_exec(Writer.get(), "COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK;
    CHECK(ready, "the independent writer releases migration admission");
    return ready;
  }

  [[nodiscard]] bool IsIndex() const {
    const auto loaded = Database->Load(Key, size_t{64} * 1024 * 1024);
    return loaded && *loaded &&
           DecodePreparedStructureIndex((**loaded).Bytes(), size_t{128} * 1024 * 1024).has_value();
  }
};

bool Busy(const StructureBakeError &error) {
  const auto *kind = std::get_if<StructureBakeErrorKind>(&error);
  return kind != nullptr && *kind == StructureBakeErrorKind::ArtifactBusy;
}

bool Resume(StructureBuildTask &task, Tasks &pool, const BuildingMesh &mesher) {
  const auto until = std::chrono::steady_clock::now() + 5s;
  while (!task.CanResume() && task.Result().Status && !task.Result().Tile &&
         std::chrono::steady_clock::now() < until) {
    std::this_thread::yield();
  }
  if (!task.CanResume()) { return false; }
  task.Resume(pool, mesher);
  task.Join(pool);
  return true;
}

void CursorKeepsCompletedPhases() {
  Fixture fixture;
  if (!fixture.Seed() || !fixture.Begin()) { return; }
  const auto before = fixture.Cache->Costs();
  const std::atomic_bool stopping{false};
  std::optional<PreparedBuildingAssets::BaseWrite> migration;
  auto first = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
  CHECK(!first && Busy(first.error()) && migration && migration->PublicationBytes() > 0,
        "a blocked legacy read retains its decoded product and first owned cell package");
  const auto bytes = migration ? migration->PublicationBytes() : 0;
  auto repeated = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
  CHECK(!repeated && Busy(repeated.error()) && migration &&
            migration->PublicationBytes() == bytes &&
            fixture.Cache->Costs().LegacyPreparations == before.LegacyPreparations + 1 &&
            fixture.Cache->Costs().BasePackagePreparations == before.BasePackagePreparations + 1,
        "repeated Busy never decodes the complete legacy product or encodes its cell twice");
  const auto different = fixture.Cache->ReadBase(fixture.Key + "other", migration, stopping);
  CHECK(!different && std::get_if<StructureBakeErrorKind>(&different.error()) &&
            *std::get_if<StructureBakeErrorKind>(&different.error()) ==
                StructureBakeErrorKind::ArtifactInvalidProduct,
        "an existing continuation cannot be silently rebound to another product key");
  if (!fixture.Commit()) { return; }
  const auto cell = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
  CHECK(cell && !*cell && migration && !fixture.IsIndex(),
        "one bounded step publishes only the retained cell and leaves the original logical parent "
        "valid");
  if (!fixture.Begin()) { return; }
  const auto index = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
  const auto indexAgain = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
  CHECK(!index && Busy(index.error()) && !indexAgain && Busy(indexAgain.error()) &&
            fixture.Cache->Costs().BasePackagePreparations == before.BasePackagePreparations + 2 &&
            fixture.Cache->Costs().LegacyPreparations == before.LegacyPreparations + 1,
        "a later blocked index phase retains both prior cell progress and its own encoded package");
  if (!fixture.Commit()) { return; }
  PreparedBuildingAssets::Base ready;
  for (unsigned step = 0; step < 16 && !ready; ++step) {
    auto result = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
    CHECK(result.has_value(), "each remaining bounded native migration step succeeds");
    if (!result) { return; }
    ready = std::move(*result);
  }
  CHECK(ready && fixture.IsIndex() &&
            fixture.Cache->Costs().BasePackagePreparations == before.BasePackagePreparations + 3 &&
            fixture.Cache->Costs().BasePreparations == before.BasePreparations,
        "the same key reaches the native index after exactly one cell, index and basis preparation "
        "without sources");
  const BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  auto referenceScratch = mesher.Scratch();
  RawTile view;
  view.RequestedDetail = LevelOfDetail::Shell;
  view.RequestedCell = fixture.Complete.Structures.front().Layout.Cell.Index;
  if (!ready) { return; }
  const auto baked = BakePreparedStructures(*ready, view, mesher, *scratch);
  const auto reference = BakePreparedStructures(fixture.Complete, view, mesher, *referenceScratch);
  CHECK(baked && reference && baked->Digest == reference->Digest &&
            baked->Prints == reference->Prints,
        "resumable migration preserves complete geometry, materials and terrain contacts");
}

void TaskOwnsLegacyMigration() {
  Fixture fixture;
  if (!fixture.Seed() || !fixture.Begin()) { return; }
  const auto before = fixture.Cache->Costs();
  auto view = std::make_unique<RawTile>();
  view->RequestedDetail = LevelOfDetail::Shell;
  view->RequestedCell = fixture.Complete.Structures.front().Layout.Cell.Index;
  const BuildingMesh mesher;
  Tasks pool(1);
  StructureBuildTask task(0,
                          fixture.Basis,
                          std::move(view),
                          std::make_unique<StructureBuildTask::Output>(),
                          mesher.Scratch());
  task.UseNativeAssets(fixture.Cache, fixture.Key);
  task.Start(pool, mesher);
  task.Join(pool);
  CHECK(task.Result().Status && !task.Result().Tile && task.Result().PublicationBytes > 0 &&
            task.PreparedBase() == nullptr,
        "the real basis-backed task owns the deferred native conversion instead of a temporary "
        "Load local");
  CHECK(Resume(task, pool, mesher) &&
            fixture.Cache->Costs().LegacyPreparations == before.LegacyPreparations + 1 &&
            fixture.Cache->Costs().BasePackagePreparations == before.BasePackagePreparations + 1,
        "another actual task submission retains the original migration and prepared cell bytes");
  const auto sentinel = pool.Post([] {});
  pool.Wait(sentinel);
  CHECK(task.Result().Status && !task.Result().Tile,
        "paused native conversion leaves the only worker available to unrelated work");
  if (!fixture.Commit()) { return; }
  for (unsigned step = 0; step < 32 && task.Result().Status && !task.Result().Tile; ++step) {
    if (!Resume(task, pool, mesher)) { break; }
  }
  CHECK(task.Result().Status && task.Result().Tile && fixture.IsIndex() && task.PreparedBase() &&
            fixture.Cache->Costs().LegacyPreparations == before.LegacyPreparations + 1 &&
            fixture.Cache->Costs().BasePreparations == before.BasePreparations &&
            fixture.Cache->Costs().BasePackagePreparations == before.BasePackagePreparations + 3,
        "the real job completes index migration and geometry without regenerating native source "
        "inputs");
  CHECK(task.Result().PublicationBytes == 0,
        "completion releases the task's retained publication package capacity");
}

void StopKeepsTheOriginalParent() {
  Fixture fixture;
  if (!fixture.Seed() || !fixture.Begin()) { return; }
  std::optional<PreparedBuildingAssets::BaseWrite> migration;
  std::atomic_bool stopping{false};
  const auto first = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
  CHECK(!first && Busy(first.error()) && migration,
        "the canceled read begins with an owned paused native package");
  const auto attempts = migration ? migration->PublicationAttempts() : 0;
  stopping.store(true);
  const auto stopped = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
  CHECK(!stopped && std::get_if<StructureBakeErrorKind>(&stopped.error()) &&
            *std::get_if<StructureBakeErrorKind>(&stopped.error()) ==
                StructureBakeErrorKind::Cancelled &&
            !migration,
        "stopping drops the native continuation and admits no further write attempt");
  CHECK(attempts == 1 && !fixture.IsIndex(),
        "the original complete parent remains valid after cancellation");
  if (!fixture.Commit()) { return; }
  const auto original = fixture.Database->Load(fixture.Key, size_t{64} * 1024 * 1024);
  CHECK(original && *original &&
            DecodePreparedStructureTile((**original).Bytes(), size_t{128} * 1024 * 1024),
        "canceling native conversion never removes or partially replaces the readable legacy "
        "product");
}
}

int main() {
  CursorKeepsCompletedPhases();
  TaskOwnsLegacyMigration();
  StopKeepsTheOriginalParent();
  return Report();
}
