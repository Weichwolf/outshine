#include "test/harness/engine/NativeBuildingSources.h"
#include "JobAdmissionFixture.h"
#include "StructureBuildTask.h"
#include "BuildingMesh.h"
#include "PreparedBuildingAssets.h"
#include "PreparedBuildingBasis.h"
#include "PreparedStructureTile.h"
#include "StructureBake.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "AssetCache.h"
#include "Check.h"
#include "Sha256.h"
#include "Tasks.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Generators;

bool Complete(StructureBuildTask &task, Tasks &workers, std::shared_ptr<BuildingMesh> mesher) {
  task.Start(workers, std::move(mesher));
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    task.Join();
    if (!task.Result().Status || task.Result().Tile) { return task.Result().Status.has_value(); }
    if (task.CanResume()) {
      task.Resume(workers);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}

StructureBuildTask Request(const std::shared_ptr<PreparedBuildingAssets> &assets,
                           const PreparedBuildingAssets::Basis &basis,
                           const std::string &key) {
  const auto mesher = std::make_shared<BuildingMesh>();
  StructureBuildTask task(JobLease(),
                          0,
                          basis,
                          std::make_unique<RawTile>(),
                          std::make_unique<StructureBuildTask::Output>(),
                          mesher->Scratch());
  task.UseTerrainContacts();
  task.UseNativeAssets(assets, key);
  return task;
}

void EmptyLand(PreparedBuildingAssets &assets, const Vec3 &anchor) {
  PreparedStructureTile empty;
  empty.AnchorEcef = anchor;
  auto contacts = StructureTerrainContacts(empty);
  CHECK(contacts, "an empty countryside tile has a complete contact result");
  if (!contacts) { return; }
  const auto key = Sha256Hex("empty-countryside-contacts");
  auto prepared = assets.PrepareTerrainContacts(key, empty, *contacts);
  CHECK(prepared && assets.PublishProduct(*prepared), "known empty contacts are stored");
  const auto basis =
      std::make_shared<const PreparedBuildingBasis>(PreparedBuildingBasis::Of(empty));
  auto loaded = assets.LoadTerrainContacts(key, basis);
  CHECK(loaded && *loaded && (**loaded).Prints.empty() && (**loaded).OccupiedCells == 0,
        "returning to empty land reuses its asset instead of rebuilding buildings");
}

void Replay(const BuildingSources::Fixture &fixture,
            const std::string &key,
            const PreparedStructureTile &base,
            const BakedTile &expected,
            const PreparedBuildingAssets::Basis &basis) {
  auto opened = AssetCache::Open((fixture.Directory.Path / "contacts-only.sqlite").string());
  CHECK(opened, "the saved contact assets have an independent store");
  if (!opened) { return; }
  const std::shared_ptr<AssetCache> store(std::move(*opened));
  Data::RawSourceCache raw({.Directory = (fixture.Directory.Path / "missing-sources").string()});
  const Data::SourceSet sources(raw);
  auto assets = PreparedBuildingAssets::Open(store, sources);
  CHECK(assets, "the consumer opens without providers or prepared building data");
  if (!assets) { return; }
  auto publication = (*assets)->PrepareTerrainContacts(key, base, expected);
  CHECK(publication && (*assets)->PublishProduct(*publication), "complete contacts are persisted");
  if (!publication) { return; }
  const auto index = store->Find(key);
  CHECK(index && !*index, "the full building index is absent from the saved game assets");
  auto task = Request(*assets, basis, key);
  Tasks workers(1);
  CHECK(Complete(task, workers, std::make_shared<BuildingMesh>()) && task.Result().Tile &&
            task.Result().CacheHit,
        "a fresh world loads contacts without rebuilding source plans");
  const auto &result = task.Result();
  if (!result.Tile) { return; }
  const auto &actual = *result.Tile;
  CHECK(actual.Prints == expected.Prints && actual.AcrossM == expected.AcrossM &&
            actual.SeatSpreadM == expected.SeatSpreadM &&
            actual.OccupiedCells == expected.OccupiedCells,
        "terrain deformation and building placement retain their functional contact data");
  CHECK(actual.Coordinates && actual.Coordinates->Points().data() == basis->PointsLatLon.data(),
        "the contact consumer shares coordinate storage with the loaded basis");
  const auto costs = (*assets)->Costs();
  CHECK(costs.ContactHits == 1 && costs.ReadBytes == 0 && costs.SurfaceReadBytes == 0 &&
            costs.GeometryPreparations == 0,
        "a saved contact hit decodes neither the full index nor source forms or render meshes");
  auto changed = (*assets)->LoadTerrainContacts(Sha256Hex(key + "/new-terrain"), basis);
  CHECK(changed && !*changed, "another terrain version cannot reuse the old contact asset");
  const auto record = store->Find(publication->Key);
  CHECK(record && *record, "the persisted contact record exists");
  if (!record || !*record) { return; }
  auto damaged = **record;
  damaged.Package.clear();
  damaged.OffsetBytes = 0;
  damaged.ByteCount = 2;
  const std::array<uint8_t, 2> bytes{1, 2};
  CHECK(store->Publish(std::span(&damaged, 1), bytes), "a truncated cache entry is reproducible");
  auto corrupt = (*assets)->LoadTerrainContacts(key, basis);
  CHECK(corrupt && !*corrupt, "corrupt contacts are a miss, never a partial functional world");
  EmptyLand(**assets, base.AnchorEcef);
}
}

int main() {
  BuildingSources::Fixture fixture;
  const bool opened = fixture.Open();
  const auto key = opened ? fixture.Generate() : std::nullopt;
  CHECK(key, "native building inputs are available");
  if (!key) { return Report(); }
  const auto assets = fixture.Stack.BuildingAssets();
  const auto base = assets->Load(*key);
  const auto basis = assets->LoadBasis(*key);
  CHECK(base && *base && basis && *basis, "the original source and coordinate basis load");
  if (!base || !*base || !basis || !*basis) { return Report(); }
  auto expected = StructureTerrainContacts(**base);
  CHECK(expected, "the uncached contact result is available for the game-load comparison");
  if (!expected) { return Report(); }
  Tasks workers(1);
  auto initial = Request(assets, *basis, *key);
  CHECK(Complete(initial, workers, std::make_shared<BuildingMesh>()) && initial.Result().Tile &&
            !initial.Result().CacheHit && assets->Costs().ContactWrites == 1,
        "a cache miss generates and persists contacts through the normal worker path");
  Replay(fixture, *key, **base, *expected, *basis);
  return Report();
}
