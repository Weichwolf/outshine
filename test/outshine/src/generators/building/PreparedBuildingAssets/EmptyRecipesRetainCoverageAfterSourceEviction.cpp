#include "PreparedBuildingAssets.h"
#include "PageIndexAsset.h"
#include "OpenAssetStore.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "HeightField.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using SourceState = PreparedBuildingAssets::SourceMember::State;
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-empty-recipes-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    const auto cache = OpenAssetStore(root.string());
    Data::RawSourceCache rawSources({.Directory = (root / "sources").string()});
    const Data::SourceSet sources(rawSources);
    auto assets = PreparedBuildingAssets::Open(cache, sources);
    CHECK(assets, "the source and asset stores open");
    if (!assets) { return Report(); }
    Ground::HeightField::Block block;
    block.At = {.Zoom = 0, .X = 0, .Y = 0};
    block.Raster = {.Side = 2, .Postings = 2};
    block.Nodes = {100, 100, 100, 100};
    const auto heights = Ground::HeightField::Of(0, {block});
    CHECK(heights, "the cell has known ground heights");
    if (!heights) { return Report(); }
    RawTile raw;
    raw.TileSpanM = 1000;
    GeoToEcef({.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 100}, raw.AnchorEcef);
    for (uint32_t at = 0; at < 2; ++at) {
      const double width = at == 0 ? .0002 : .00000001;
      const double lon = 16.001 + at * .002;
      raw.LatLon.insert(
          raw.LatLon.end(),
          {48.001, lon, 48.001, lon + width, 48.001 + width, lon + width, 48.001 + width, lon});
      raw.Structures.push_back({.LocalFirst = at * 4,
                                .PointCount = 4,
                                .Cell = {.Index = 1, .Footprint = {}},
                                .HeightM = 30,
                                .WallColour = {},
                                .Facade = {}});
    }
    const auto key = (*assets)->Key({.Zoom = 0, .X = 0, .Y = 0}, 0, {}, 1000, std::string(64, 'a'));
    const auto base = (*assets)->Generate(key, raw, *heights, std::atomic_bool{false});
    CHECK(base && *base, "a building and a recipe omission prepare in the same game cell");
    if (!base || !*base) { return Report(); }
    const auto bytes = PageIndexAsset::EncodedBytes((*base)->Structures.size());
    CHECK(bytes, "source metadata has a count-derived storage bound");
    if (!bytes) { return Report(); }
    const auto first =
        (*assets)->DescribeSourceGroups(key, {.Occupied = 1, .IndexBytesMost = *bytes});
    CHECK(first && *first && (**first).Members.size() == 2 &&
              (**first).Members[0].Availability == SourceState::Surface &&
              (**first).Members[1].Availability == SourceState::Empty &&
              (**first).Members[1].Faces == 0,
          "metadata separates the drawable source from its confirmed omitted neighbor");
    CHECK(cache->Remove(key), "the prepared source is evicted independently of its metadata");
    assets->reset();
    auto fresh = PreparedBuildingAssets::Open(cache, sources);
    CHECK(fresh, "a fresh consumer opens without the prepared source");
    if (!fresh) { return Report(); }
    const auto hit = (*fresh)->DescribeSourceGroups(key, {.Occupied = 1, .IndexBytesMost = *bytes});
    CHECK(hit && *hit && (**hit).Members.size() == 2 &&
              (**hit).Members[1].Availability == SourceState::Empty &&
              (*fresh)->Costs().ReadBytes == 0 && (*fresh)->Costs().SurfaceReadBytes == 0,
          "a source-free cache hit retains the empty recipe proof without opening source data");
  }
  std::filesystem::remove_all(root);
  return Report();
}
