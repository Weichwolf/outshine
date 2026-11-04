#include "PreparedBuildingAssets.h"
#include "PageIndexAsset.h"
#include "OpenAssetStore.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "HeightField.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
