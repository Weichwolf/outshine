#include "AssetCache.h"
#include "BuildingMesh.h"
#include "PreparedBuildingAssets.h"
#include "PreparedBuildingBasis.h"
#include "PreparedStructureTile.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "test/harness/shared/BuildingAssetFixture.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kBytesMost = size_t{1024} * 1024;

void CheckRecords(PreparedBuildingAssets &cache,
                  AssetCache &store,
                  const PreparedStructureTile &base,
                  const RawTile &view) {
  const std::string key(64, 'a');
  const BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  const auto built = BakePreparedStructures(base, view, mesher, *scratch);
  CHECK(built, "the fixture builds actual native geometry from its complete basis");
  if (!built) { return; }
  const auto geometry = BuildingAssetFixture::StoreGeometry(cache, key, base, view, *built);
  CHECK(geometry, "the real geometry producer publishes its existing asset record");
  if (!geometry) { return; }
  const auto original = store.Load(*geometry, kBytesMost);
  const auto basis = std::make_shared<const PreparedBuildingBasis>(PreparedBuildingBasis::Of(base));
  const auto hit = cache.LoadGeometry(key, base, view);
  const auto basisHit = cache.LoadGeometry(key, basis, view);
  CHECK(original && *original && hit && *hit && basisHit && *basisHit,
        "the correctly bound source and representation load before each counterexample");
  if (!original || !*original) { return; }
  std::array<AssetRecord, 4> invalid;
  invalid.fill((**original).Record());
  invalid[0].Parent = std::string(64, 'b');
  invalid[1].Parent.clear();
  invalid[2].Kind = "other-native-product";
  ++invalid[3].Level;
  for (auto record : invalid) {
    record.Package.clear();
    record.OffsetBytes = 0;
    CHECK(store.Publish({&record, 1}, (**original).Bytes()),
          "only record semantics change: the exact original payload retains a valid package CRC");
    const auto wrong = store.Load(*geometry, kBytesMost);
    CHECK(wrong && *wrong && (**wrong).Record().Parent == record.Parent &&
              (**wrong).Record().Kind == record.Kind && (**wrong).Record().Level == record.Level,
          "the independent asset reader observes the substituted record");
    const auto before = cache.Costs();
    const auto refused = cache.LoadGeometry(key, base, view);
    const auto refusedBasis = cache.LoadGeometry(key, basis, view);
    const auto after = cache.Costs();
    CHECK(refused && !*refused && refusedBasis && !*refusedBasis &&
              after.GeometryHits == before.GeometryHits &&
              after.GeometryMisses == before.GeometryMisses + 2,
          "another source or representation cannot become a valid geometry cache hit");
    const auto repaired = BuildingAssetFixture::StoreGeometry(cache, key, base, view, *built);
    const auto restored = cache.LoadGeometry(key, base, view);
    const auto restoredBasis = cache.LoadGeometry(key, basis, view);
    CHECK(repaired && *repaired == *geometry && restored && *restored && restoredBasis &&
              *restoredBasis,
          "the existing producer repairs the same asset address without a second cache");
  }
}

void Exercise(const std::filesystem::path &directory) {
  Data::RawSourceCache raw({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  const Data::SourceSet sources(raw);
  auto cache = PreparedBuildingAssets::Open(directory.string(), sources);
  auto store = AssetCache::Open((directory / "assets.sqlite").string());
  const auto heights = BuildingAssetFixture::Heights();
  CHECK(cache && store && heights, "the native asset cache and height inputs open");
  if (!cache || !store || !heights) { return; }
  const auto base = PrepareStructureTile(BuildingAssetFixture::Inputs(), *heights);
  CHECK(base && !base->Structures.empty(), "one real building is fully prepared");
  if (!base || base->Structures.empty()) { return; }
  RawTile view;
  view.Eye = {.LongitudeDeg = 9.001, .LatitudeDeg = 47.001};
  view.Projection = {.FocalPx = 720, .AllowedErrorPx = 1};
  CheckRecords(**cache, **store, *base, view);
  view.RequestedDetail = LevelOfDetail::Fine;
  CheckRecords(**cache, **store, *base, view);
  view.RequestedDetail = LevelOfDetail::Shell;
  view.RequestedCell = base->Structures.front().Layout.Cell.Index;
  CheckRecords(**cache, **store, *base, view);
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-building-geometry-record-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Exercise(directory);
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "all native owners release the private fixture before cleanup");
  return Report();
}
