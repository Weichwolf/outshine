#include "PreparedBuildingAssets.h"
#include "PageIndexAsset.h"
#include "OpenAssetStore.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "HeightField.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include "ByteArchive.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

RawTile Inputs() {
  RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 100}, raw.AnchorEcef);
  for (uint32_t source = 0; source < 2; ++source) {
    const double lon = 16.001 + source * 0.002;
    raw.LatLon.insert(raw.LatLon.end(),
                      {48.001, lon, 48.001, lon + 0.0002, 48.0012, lon + 0.0002, 48.0012, lon});
    raw.Structures.push_back({.LocalFirst = source * 4,
                              .PointCount = 4,
                              .SourceFirst = source * 4,
                              .Cell = {.Index = 1, .Footprint = {}},
                              .HeightM = 30,
                              .Pitched = 0,
                              .WallColour = {},
                              .Facade = {}});
  }
  return raw;
}

std::vector<uint32_t> NearFirst(const PageIndexAsset &index, const Vec3 &eye) {
  std::vector<uint32_t> order;
  CHECK(index.Hierarchy.SelectNodesNearFirst(
            eye,
            [](const auto &) { return false; },
            [&](const auto &node) { order.push_back(index.Hierarchy.SourceOrder()[node.First]); }),
        "source ordinals can be prioritized using only the retained metadata");
  return order;
}

void MalformedGroups(AssetCache &cache,
                     PreparedBuildingAssets &owner,
                     const std::string &key,
                     const Box &bounds,
                     size_t bytes) {
  const auto rows = cache.Select({.Bounds = bounds,
                                  .Kind = "building-source-groups",
                                  .Level = std::nullopt,
                                  .Radius = std::nullopt,
                                  .Planes = {},
                                  .Parent = key});
  CHECK(rows && rows->size() == 1, "one revision-bound metadata product exists");
  if (!rows || rows->size() != 1) { return; }
  auto record = rows->front();
  auto loaded = cache.Load(record.Key, bytes);
  CHECK(loaded && *loaded, "the compact metadata body is available independently");
  if (!loaded || !*loaded) { return; }
  const auto encoded = (**loaded).Bytes();
  CHECK(encoded.size() == 12 + 2 * 17,
        "metadata carries two seventeen-byte members and a versioned header");
  if (encoded.size() != 46) { return; }
  std::vector<std::vector<uint8_t>> invalid(9, {encoded.begin(), encoded.end()});
  invalid[0].pop_back();
  invalid[1][0] ^= 1;
  invalid[2][12] = 0;
  invalid[3][16] = 2;
  invalid[4][8] = 255;
  for (size_t at = 17; at < 21; ++at) { invalid[5][at] = 0; }
  const auto negative = LittleEndian(-0.5);
  const auto nan = LittleEndian(std::numeric_limits<double>::quiet_NaN());
  std::memcpy(invalid[6].data() + 21, &negative, sizeof(negative));
  std::memcpy(invalid[7].data() + 21, &nan, sizeof(nan));
  invalid[8].push_back(0);
  loaded->reset();
  record.Package.clear();
  record.OffsetBytes = 0;
  for (const auto &payload : invalid) {
    CHECK(cache.Remove(record.Key), "the prior test metadata is evicted before replacement");
    record.ByteCount = payload.size();
    CHECK(cache.Publish(std::span(&record, 1), payload), "the malformed counterexample is stored");
    CHECK(!owner.DescribeSourceGroups(key, {.Occupied = 1, .IndexBytesMost = bytes}),
          "invalid source metadata fails before source fallback");
  }
}

void CachedGroups(const std::filesystem::path &root,
                  const Data::SourceSet &sources,
                  const std::string &key,
                  size_t bytes) {
  const auto cache = OpenAssetStore(root.string());
  auto owner = PreparedBuildingAssets::Open(cache, sources);
  CHECK(owner, "a fresh metadata-only owner opens after source eviction");
  if (!owner) { return; }
  const auto hit = (*owner)->DescribeSourceGroups(key, {.Occupied = 1, .IndexBytesMost = bytes});
  CHECK(hit && *hit && (**hit).Members.size() == 2 && (**hit).Cells.empty(),
        "cached cohorts survive absent headers without inventing complete cell products");
  CHECK((*owner)->Costs().ReadBytes == 0 && (*owner)->Costs().SurfaceReadBytes == 0,
        "fresh group hits decode neither prepared headers nor form blocks");
  if (hit && *hit) {
    for (const auto &member : (**hit).Members) {
      CHECK(member.Cell == 1 && member.FieldSupported && member.Faces > 0 && member.ShellErrorM &&
                *member.ShellErrorM >= 0,
            "cached sources retain native support and declared geometry errors without source IO");
    }
    MalformedGroups(*cache, **owner, key, (**hit).Index->Hierarchy.Nodes().front().Bounds, bytes);
  }
}

void Exercise(const std::filesystem::path &root) {
  const auto cache = OpenAssetStore(root.string());
  Data::RawSourceCache rawSources({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(rawSources);
  auto assets = PreparedBuildingAssets::Open(cache, sources);
  CHECK(assets, "the building store opens on the shared asset database");
  if (!assets) { return; }
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100, 100, 100, 100};
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights, "the source fixture has valid heights");
  if (!heights) { return; }
  const auto raw = Inputs();
  const auto key = (*assets)->Key({.Zoom = 0, .X = 0, .Y = 0}, 0, {}, 1000, std::string(64, 'a'));
  const std::atomic_bool stop{false};
  const auto base = (*assets)->Generate(key, raw, *heights, stop);
  CHECK(base && *base, "two native source buildings prepare in the same cell");
  if (!base || !*base) { return; }
  const auto bytes = PageIndexAsset::EncodedBytes((*base)->Structures.size());
  CHECK(bytes, "the hierarchy reservation derives from its native source count");
  if (!bytes) { return; }
  const auto before = (*assets)->Costs();
  const auto described = (*assets)->DescribeSourceCells(key, 1);
  CHECK(described && described->size() == 1 && described->front().Cell == 1 &&
            described->front().NativeBytes > 0 && described->front().HeaderBytes > 0 &&
            (*assets)->Costs().ReadBytes == before.ReadBytes &&
            (*assets)->Costs().SurfaceReadBytes == before.SurfaceReadBytes,
        "cell envelopes and native IO sizes resolve from metadata without body reads");
  CHECK(!(*assets)->ResolveSourceIndex(key, *bytes - 1),
        "an insufficient hierarchy reservation is refused");
  const auto index = (*assets)->ResolveSourceIndex(key, *bytes);
  CHECK(index && *index && (*index)->From == PageIndexAsset::Origin::Created &&
            (*index)->Hierarchy.SourceOrder().size() == 2,
        "one index miss publishes a source hierarchy without meshing");
  CHECK((*assets)->Costs().SurfaceReadBytes == before.SurfaceReadBytes &&
            (*assets)->Costs().BasePreparations == before.BasePreparations &&
            (*assets)->Costs().GeometryPreparations == before.GeometryPreparations,
        "hierarchy construction opens no form blocks and regenerates no source or geometry");
  if (!index || !*index) { return; }
  const auto groups =
      (*assets)->DescribeSourceGroups(key, {.Occupied = 1, .IndexBytesMost = *bytes});
  CHECK(groups && *groups && (**groups).Members.size() == 2 && (**groups).Cells.size() == 1 &&
            (**groups).Index->Hierarchy.SourceOrder().size() == 2,
        "group discovery retains every source and its persisted hierarchy");
  if (groups && *groups) {
    for (const auto &member : (**groups).Members) {
      CHECK(
          member.Cell == 1 && member.FieldSupported && member.Faces > 0 && member.ShellErrorM &&
              *member.ShellErrorM >= 0,
          "native headers expose cohort, geometry support and declared shell error without forms");
    }
  }
  CHECK((*assets)->Costs().SurfaceReadBytes == before.SurfaceReadBytes &&
            (*assets)->Costs().SurfacePreparations == before.SurfacePreparations &&
            (*assets)->Costs().GeometryPreparations == before.GeometryPreparations,
        "group discovery neither decodes forms nor constructs render geometry");
  const Vec3 eye = raw.AnchorEcef + (*base)->Surfaces[1].Bounds().Middle();
  const auto order = NearFirst(**index, eye);
  CHECK(order == std::vector<uint32_t>({1, 0}), "metadata selects the nearest original ordinal");
  auto formRecords = cache->Select({.Bounds = (*index)->Hierarchy.Nodes().front().Bounds,
                                    .Kind = "building-forms",
                                    .Level = std::nullopt,
                                    .Radius = std::nullopt,
                                    .Planes = {},
                                    .Parent = key});
  CHECK(formRecords, "all source form records are enumerated without payload IO");
  if (!formRecords) { return; }
  for (const auto &record : *formRecords) {
    CHECK(cache->Remove(record.Key), "source form evicted");
  }
  CHECK(cache->Remove(key), "the prepared source headers are evicted as well");
  const auto freshCache = OpenAssetStore(root.string());
  auto fresh = PreparedBuildingAssets::Open(freshCache, sources);
  CHECK(fresh, "a fresh building owner opens after all prepared inputs are gone");
  if (!fresh) { return; }
  const auto hit = (*fresh)->ResolveSourceIndex(key, *bytes);
  CHECK(hit && *hit && (*hit)->From == PageIndexAsset::Origin::Stored &&
            NearFirst(**hit, eye) == order,
        "the source-free hit retains complete ordinal coverage and spatial priority");
  const auto costs = (*fresh)->Costs();
  CHECK(costs.ReadBytes == 0 && costs.SurfaceReadBytes == 0 && costs.GeometryReadBytes == 0 &&
            costs.BasePreparations == 0 && costs.GeometryPreparations == 0,
        "fresh hierarchy hits require neither prepared input reads nor generation");
  CachedGroups(root, sources, key, *bytes);
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-tests.building-source-index-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Exercise(root);
  std::filesystem::remove_all(root);
  return outshine::Test::Report();
}
