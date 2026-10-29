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
    if (!ReplaceWithLegacy()) { return false; }
    sqlite3 *writer = nullptr;
    const auto status = sqlite3_open((Root / "assets.sqlite").c_str(), &writer);
    Writer.reset(writer);
    CHECK(status == SQLITE_OK, "a second real connection controls writer admission");
    return status == SQLITE_OK;
  }

  [[nodiscard]] bool ReplaceWithLegacy() const {
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
    return published.has_value();
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

void BasisReadReusesTheMigrationBasis() {
  Fixture fixture;
  if (!fixture.Seed()) { return; }
  const auto base = fixture.Database->Find(fixture.Key);
  CHECK(base && *base, "the complete native parent supplies conservative query bounds");
  if (!base || !*base) { return; }
  const auto records = fixture.Database->Select({.Bounds = (**base).Bounds,
                                                 .Kind = "building-basis",
                                                 .Level = std::nullopt,
                                                 .Radius = std::nullopt,
                                                 .Planes = {}});
  CHECK(records && records->size() == 1 && records->front().Parent == fixture.Key,
        "the private fixture identifies its sole basis without duplicating key construction");
  if (!records || records->size() != 1) { return; }
  CHECK(fixture.Database->Remove(records->front().Key),
        "only the native basis is removed before reading the legacy parent");
  const auto before = fixture.Cache->Costs();
  PreparedBuildingAssets::BasisRead read;
  const std::atomic_bool stopping{false};
  PreparedBuildingAssets::Basis ready;
  for (unsigned step = 0; step < 16 && !ready; ++step) {
    auto result = fixture.Cache->ReadBasis(fixture.Key, read, stopping);
    CHECK(result.has_value(), "the owned basis read completes each bounded native step");
    if (!result) { return; }
    ready = std::move(*result);
  }
  const auto after = fixture.Cache->Costs();
  CHECK(ready && !read.Pending() && fixture.IsIndex() &&
            after.LegacyPreparations == before.LegacyPreparations + 1 &&
            after.BasePackagePreparations == before.BasePackagePreparations + 3 &&
            after.BasisPreparations == before.BasisPreparations + 1,
        "a basis already published by legacy migration is loaded without another reconstruction or "
        "publication");
  const auto mismatch = fixture.Cache->ReadBasis(fixture.Key + "other", read, stopping);
  CHECK(!mismatch && std::get_if<StructureBakeErrorKind>(&mismatch.error()) &&
            *std::get_if<StructureBakeErrorKind>(&mismatch.error()) ==
                StructureBakeErrorKind::ArtifactInvalidProduct,
        "a completed owned read still rejects a different content key");
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

void RestoreKeepsPublicationBoundAfterLegacyReplacement() {
  Fixture fixture;
  if (!fixture.Seed()) { return; }
  const std::atomic_bool stopping{false};
  std::optional<PreparedBuildingAssets::BaseWrite> migration;
  for (unsigned step = 0; step < 3; ++step) {
    const auto advanced = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
    CHECK(advanced && !*advanced && migration,
          "the original cell/index/basis steps leave an owned final restore");
    if (!advanced || *advanced || !migration) { return; }
  }
  CHECK(fixture.IsIndex(), "the first writer has committed its native index before restoration");
  const auto attempts = migration ? migration->PublicationAttempts() : 0;
  const bool published = fixture.ReplaceWithLegacy();
  CHECK(
      published && !fixture.IsIndex(),
      "another valid writer atomically replaces the logical key with the supported legacy format");
  if (!published) { return; }
  const auto before = fixture.Cache->Costs();
  const auto restored = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
  const auto after = fixture.Cache->Costs();
  Note("restore_nested_package_preparations",
       static_cast<double>(after.BasePackagePreparations - before.BasePackagePreparations),
       "packages");
  Note("restore_nested_legacy_preparations",
       static_cast<double>(after.LegacyPreparations - before.LegacyPreparations),
       "products");
  CHECK(after.BasePackagePreparations - before.BasePackagePreparations <= 1,
        "one outer ReadBase step never hides a complete multi-package legacy migration");
  CHECK(restored.has_value(), "the complete supported product remains a valid migration input");
  CHECK(migration && migration->PublicationAttempts() == attempts + 1,
        "the same logical continuation preserves its cumulative publication attempts");
  PreparedBuildingAssets::Base ready = restored ? *restored : nullptr;
  for (unsigned step = 0; step < 16 && !ready; ++step) {
    auto result = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
    CHECK(result.has_value(), "the replacement continues through bounded native phases");
    if (!result) { return; }
    ready = std::move(*result);
  }
  CHECK(ready && fixture.IsIndex(), "the replacement finishes the original index migration");
}

void RestoreRetainsRequestAcrossReplacement(bool cancel) {
  Fixture fixture;
  if (!fixture.Seed()) { return; }
  const auto heights = Heights();
  std::atomic_bool stopping{false};
  const std::string request(64, 'd');
  auto cursor = fixture.Cache->PrepareBase(fixture.Key, Inputs(), *heights, stopping, request);
  CHECK(cursor.has_value(), "the native publication owns an explicit request alias");
  if (!cursor) { return; }
  for (unsigned step = 0; step < 3; ++step) {
    const auto result = fixture.Cache->ResumeBase(*cursor, stopping);
    CHECK(result && !*result, "cell/index/basis publication yields before the final restore");
    if (!result || *result) { return; }
  }
  const auto alias = fixture.Database->FindRequest(request);
  CHECK(alias && *alias && (**alias).Parent == fixture.Key,
        "the explicit request names the initially published native basis");
  if (!alias || !*alias) { return; }
  const auto removed = fixture.Database->Remove((**alias).Key);
  const auto absent = fixture.Database->FindRequest(request);
  CHECK(removed && absent && !*absent && fixture.IsIndex(),
        "removing the basis retires its alias while retaining the valid parent index");
  if (!removed || !absent || *absent || !fixture.ReplaceWithLegacy()) { return; }
  const auto before = cursor->PublicationAttempts();
  auto first = fixture.Cache->ResumeBase(*cursor, stopping);
  CHECK(first && !*first && cursor->PublicationAttempts() == before + 1,
        "a replacement keeps the original cursor and admits only one new publication");
  if (!first || *first) { return; }
  if (cancel) {
    stopping.store(true);
    const auto attempts = cursor->PublicationAttempts();
    const auto stopped = fixture.Cache->ResumeBase(*cursor, stopping);
    CHECK(!stopped && std::get_if<StructureBakeErrorKind>(&stopped.error()) &&
              *std::get_if<StructureBakeErrorKind>(&stopped.error()) ==
                  StructureBakeErrorKind::Cancelled &&
              cursor->PublicationAttempts() == attempts && !fixture.IsIndex(),
          "the original stop flag prevents the replacement index and basis writes");
    const auto stillAbsent = fixture.Database->FindRequest(request);
    CHECK(stillAbsent && !*stillAbsent, "canceled replacement cannot revive its request alias");
    return;
  }
  PreparedBuildingAssets::Base ready;
  for (unsigned step = 0; step < 16 && !ready; ++step) {
    auto result = fixture.Cache->ResumeBase(*cursor, stopping);
    CHECK(result.has_value(), "the same cursor completes the replacement migration");
    if (!result) { return; }
    ready = std::move(*result);
  }
  const auto restoredAlias = fixture.Database->FindRequest(request);
  CHECK(ready && restoredAlias && *restoredAlias && (**restoredAlias).Key == (**alias).Key &&
            (**restoredAlias).Parent == fixture.Key,
        "bounded restart preserves and republishes the original explicit request binding");
}

void RestoreKeepsBlockedReplacement() {
  Fixture fixture;
  if (!fixture.Seed()) { return; }
  const std::atomic_bool stopping{false};
  std::optional<PreparedBuildingAssets::BaseWrite> migration;
  for (unsigned step = 0; step < 3; ++step) {
    const auto advanced = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
    CHECK(advanced && !*advanced && migration, "the first migration reaches its final restore");
    if (!advanced || *advanced || !migration) { return; }
  }
  if (!fixture.ReplaceWithLegacy() || !fixture.Begin()) { return; }
  const auto before = fixture.Cache->Costs();
  const auto first = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
  CHECK(!first && Busy(first.error()) && migration && migration->PublicationBytes() > 0,
        "a writer-blocked replacement retains its newly decoded native product and package");
  if (!migration) { return; }
  const auto bytes = migration->PublicationBytes();
  const auto attempts = migration->PublicationAttempts();
  const auto repeated = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
  const auto paused = fixture.Cache->Costs();
  CHECK(!repeated && Busy(repeated.error()) && migration &&
            migration->PublicationBytes() == bytes &&
            migration->PublicationAttempts() == attempts + 1 &&
            paused.LegacyPreparations == before.LegacyPreparations + 1 &&
            paused.BasePackagePreparations == before.BasePackagePreparations + 1,
        "repeated Busy keeps the replacement bytes and never repeats decode or encode");
  if (!fixture.Commit()) { return; }
  PreparedBuildingAssets::Base ready;
  for (unsigned step = 0; step < 16 && !ready; ++step) {
    const auto packages = fixture.Cache->Costs().BasePackagePreparations;
    auto result = fixture.Cache->ReadBase(fixture.Key, migration, stopping);
    CHECK(result && fixture.Cache->Costs().BasePackagePreparations <= packages + 1,
          "each released replacement step admits at most one new package");
    if (!result) { return; }
    ready = std::move(*result);
  }
  const auto after = fixture.Cache->Costs();
  CHECK(ready && fixture.IsIndex() && after.LegacyPreparations == before.LegacyPreparations + 1 &&
            after.BasePackagePreparations == before.BasePackagePreparations + 3,
        "the paused replacement completes exactly one cell/index/basis migration");
}


void RestoreKeepsANewerRequestBinding() {
  Fixture fixture;
  if (!fixture.Seed()) { return; }
  const auto heights = Heights();
  const std::atomic_bool stopping{false};
  const std::string input = "owned-legacy-fixture-input";
  constexpr Data::TileId tile{.Zoom = 0, .X = 0, .Y = 0};
  const auto request = fixture.Cache->RequestKey(tile, {}, 1000, input);
  auto cursor = fixture.Cache->PrepareBase(fixture.Key, Inputs(), *heights, stopping, request);
  CHECK(cursor.has_value(), "the first producer owns its actual logical request binding");
  if (!cursor) { return; }
  for (unsigned step = 0; step < 3; ++step) {
    const auto advanced = fixture.Cache->ResumeBase(*cursor, stopping);
    CHECK(advanced && !*advanced, "the first producer completes its data before restoration");
    if (!advanced || *advanced) { return; }
  }
  const auto newerKey = fixture.Cache->Key(tile, 23, {}, 1000, input);
  const auto newer = fixture.Cache->Generate(newerKey, Inputs(), *heights, stopping, request);
  const auto winner = fixture.Database->FindRequest(request);
  CHECK(newer && *newer && winner && *winner && (**winner).Parent == newerKey &&
            newerKey != fixture.Key,
        "a current producer binds another valid street-dependent content product");
  if (!newer || !*newer || !fixture.ReplaceWithLegacy()) { return; }
  PreparedBuildingAssets::Base ready;
  for (unsigned step = 0; step < 16 && !ready; ++step) {
    auto result = fixture.Cache->ResumeBase(*cursor, stopping);
    CHECK(result.has_value(), "the older content remains independently resumable");
    if (!result) { return; }
    ready = std::move(*result);
  }
  const auto retained = fixture.Database->FindRequest(request);
  CHECK(ready && retained && *retained && (**retained).Parent == newerKey,
        "repair of older content cannot overwrite a newer valid request binding");
}

}

int main() {
  CursorKeepsCompletedPhases();
  TaskOwnsLegacyMigration();
  StopKeepsTheOriginalParent();
  RestoreKeepsPublicationBoundAfterLegacyReplacement();
  RestoreKeepsBlockedReplacement();
  RestoreKeepsANewerRequestBinding();
  RestoreRetainsRequestAcrossReplacement(false);
  RestoreRetainsRequestAcrossReplacement(true);
  BasisReadReusesTheMigrationBasis();
  return Report();
}
