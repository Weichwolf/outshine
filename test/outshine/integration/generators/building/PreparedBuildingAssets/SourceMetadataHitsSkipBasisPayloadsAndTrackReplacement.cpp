#include "AssetCache.h"
#include "BuildingSourceMetadata.h"
#include "Check.h"
#include "OpenAssetStore.h"
#include "PreparedBuildingAssets.h"
#include "PreparedStructureCodec.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "test/harness/shared/BuildingAssetFixture.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

struct Fixture {
  Data::RawSourceCache Raw{{.Directory = {}, .Using = Data::RawSourceCache::Use::Off}};
  Data::SourceSet Sources{Raw};
  std::filesystem::path Directory;
  std::shared_ptr<PreparedBuildingAssets> Cache;
  std::unique_ptr<AssetCache> Store;
  std::string Key, Request;

  std::shared_ptr<PreparedBuildingAssets> Reader() const {
    auto opened = PreparedBuildingAssets::Open(OpenAssetStore(Directory.string()), Sources);
    CHECK(opened, "a fresh reader has no retained basis or source inputs");
    return opened ? *opened : nullptr;
  }

  bool Open(const std::filesystem::path &directory) {
    Directory = directory;
    Cache = Reader();
    auto store = AssetCache::Open((directory / "assets.sqlite").string());
    const auto heights = BuildingAssetFixture::Heights();
    if (!Cache || !store || !heights) { return false; }
    Store = std::move(*store);
    const Data::TileId tile{.Zoom = 0, .X = 0, .Y = 0};
    Key = Cache->Key(tile, 17, {}, 1000, "source-metadata-inputs");
    Request = Cache->RequestKey(tile, {}, 1000, "source-metadata-inputs");
    const std::atomic_bool stopped{false};
    const auto generated =
        Cache->Generate(Key, BuildingAssetFixture::Inputs(), *heights, stopped, Request);
    return generated && *generated;
  }
};

void CheckFreshHit(const Fixture &fixture,
                   const PreparedBuildingAssets::RequestedSource &expected) {
  const auto reader = fixture.Reader();
  if (!reader) { return; }
  const auto loaded = reader->LoadSourceRequest(fixture.Request);
  CHECK(loaded && *loaded && (**loaded).BaseKey == expected.BaseKey &&
            (**loaded).Bounds == expected.Bounds &&
            (**loaded).Metadata.AnchorEcef == expected.Metadata.AnchorEcef &&
            (**loaded).Metadata.Cells == expected.Metadata.Cells &&
            (**loaded).Metadata.CellSourceCounts == expected.Metadata.CellSourceCounts &&
            (**loaded).Metadata.HeightQualified == expected.Metadata.HeightQualified,
        "a fresh source-only reader preserves all selection inputs");
  const auto costs = reader->Costs();
  CHECK(costs.SourceMetadataHits == 1 &&
            costs.SourceMetadataReadBytes == BuildingSourceMetadata::EncodedBytes &&
            costs.BasisHits == 0 && costs.BasisReadBytes == 0 && costs.ReadBytes == 0 &&
            costs.GeometryReadBytes == 0,
        "a summary hit reads exactly 297 bytes without basis, plan or geometry payloads");
}

void CheckCorruptSummary(const Fixture &fixture,
                         const PreparedBuildingAssets::RequestedSource &expected) {
  const auto basis = fixture.Store->FindRequest(fixture.Request);
  CHECK(basis && *basis, "the original source binding remains present");
  if (!basis || !*basis) { return; }
  const auto records = fixture.Store->Select({.Bounds = (**basis).Bounds,
                                              .Kind = "building-source-metadata",
                                              .Level = {},
                                              .Radius = {},
                                              .Planes = {},
                                              .Parent = (**basis).Key});
  CHECK(records && records->size() == 1, "metadata occupies an independently readable asset");
  if (!records || records->size() != 1) { return; }
  auto record = records->front();
  const auto loaded = fixture.Store->Load(record.Key, BuildingSourceMetadata::EncodedBytes);
  CHECK(loaded && *loaded, "the fixture loads only its small summary");
  if (!loaded || !*loaded) { return; }
  for (const bool oversized : {false, true}) {
    auto bytes = std::vector<uint8_t>((**loaded).Bytes().begin(), (**loaded).Bytes().end());
    if (oversized) {
      bytes.push_back(0);
    } else {
      bytes.pop_back();
    }
    record.Package.clear();
    record.ByteCount = bytes.size();
    CHECK(fixture.Store->Publish({&record, 1}, bytes),
          "a valid package CRC does not make a wrong-sized native summary valid");
    const auto reader = fixture.Reader();
    if (!reader) { return; }
    const auto repaired = reader->LoadSourceRequest(fixture.Request);
    CHECK(
        repaired && *repaired && (**repaired).Metadata.Cells == expected.Metadata.Cells &&
            reader->Costs().SourceMetadataWrites == 1 && reader->Costs().BasisHits == 1,
        "bad metadata is repaired locally from the existing native basis without lifting its cap");
  }
  CheckFreshHit(fixture, expected);
}

void CheckReplacement(const Fixture &fixture, PreparedBuildingAssets::RequestedSource expected) {
  const auto basis = fixture.Cache->LoadBasis(fixture.Key);
  auto record = fixture.Store->FindRequest(fixture.Request);
  CHECK(basis && *basis && record && *record, "the fixture retains its old native snapshot");
  if (!basis || !*basis || !record || !*record) { return; }
  auto changed = **basis;
  changed.AnchorEcef[0] += 2;
  const auto bytes = EncodePreparedBuildingBasis(changed);
  CHECK(bytes, "a changed source origin remains a valid native basis");
  if (!bytes) { return; }
  auto replacement = **record;
  replacement.Package.clear();
  replacement.ByteCount = bytes->size();
  CHECK(fixture.Store->Publish({&replacement, 1}, *bytes),
        "the same address receives a new package");
  expected.Metadata.AnchorEcef = changed.AnchorEcef;
  const auto reader = fixture.Reader();
  if (!reader) { return; }
  const auto updated = reader->LoadSourceRequest(fixture.Request);
  CHECK(updated && *updated && (**updated).Metadata.AnchorEcef == changed.AnchorEcef &&
            reader->Costs().SourceMetadataMisses == 1 && reader->Costs().SourceMetadataWrites == 1,
        "package replacement cannot reuse the previous summary under the same logical address");
  CheckFreshHit(fixture, expected);
  CHECK(fixture.Store->Remove((**record).Key), "removing the basis removes its request binding");
  const auto missingReader = fixture.Reader();
  if (!missingReader) { return; }
  const auto missing = missingReader->LoadSourceRequest(fixture.Request);
  CHECK(missing && !*missing && missingReader->Costs().SourceMetadataHits == 0,
        "an orphan summary does not fabricate a source after its binding disappears");
}

void Exercise(const std::filesystem::path &directory) {
  Fixture fixture;
  CHECK(fixture.Open(directory), "a complete native building basis is available without metadata");
  if (!fixture.Cache || !fixture.Store) { return; }
  const auto first = fixture.Cache->LoadSourceRequest(fixture.Request);
  CHECK(first && *first && fixture.Cache->Costs().SourceMetadataMisses == 1 &&
            fixture.Cache->Costs().SourceMetadataWrites == 1 &&
            fixture.Cache->Costs().BasisHits == 1,
        "existing assets migrate once without source acquisition or geometry generation");
  if (!first || !*first) { return; }
  CheckFreshHit(fixture, **first);
  CheckCorruptSummary(fixture, **first);
  CheckReplacement(fixture, **first);
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-building-source-metadata-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Exercise(directory);
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "all native owners release the private fixture before cleanup");
  return Report();
}
