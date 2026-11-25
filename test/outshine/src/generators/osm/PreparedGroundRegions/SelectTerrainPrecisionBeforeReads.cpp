#include "AssetCache.h"
#include "Check.h"
#include "GroundRegionAsset.h"
#include "OpenAssetStore.h"
#include "PreparedGroundRegions.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "StoreGroundRegion.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Regions = Generators::Osm::PreparedGroundRegions;

Sheet Hills(Data::TileId tile, float offset) {
  Sheet sheet{.Tile = tile, .Nodes = {}, .Side = 33, .Postings = 33};
  constexpr uint32_t side = 35;
  for (uint32_t at = 0; at < side * side; ++at) {
    const float height = offset + static_cast<float>((at * 17u + at / side) % 120u) * 0.125f;
    sheet.Nodes.push_back(
        std::bit_cast<float>(std::bit_cast<uint32_t>(height) | ((at * 19937u) & 255u)));
  }
  return sheet;
}

std::optional<Regions::Loaded> Restore(Regions &regions,
                                       const std::string &key,
                                       const Generators::Osm::GroundTerrainErrorLimit &limit = {}) {
  auto native = regions.LoadProduct(key);
  if (!native || !*native) { return std::nullopt; }
  auto loaded = regions.Restore(std::move(**native), {}, limit);
  return loaded ? std::move(*loaded) : std::nullopt;
}

bool SameWords(const Sheet &a, const Sheet &b) {
  return a.Tile == b.Tile && std::ranges::equal(a.Samples(), b.Samples(), [](float x, float y) {
           return std::bit_cast<uint32_t>(x) == std::bit_cast<uint32_t>(y);
         });
}
}

int main() {
  const auto path = PlantedPath("native-terrain-precision");
  std::filesystem::remove_all(path);
  const auto cache = OpenAssetStore(path);
  Data::RawSourceCache raw({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  const Data::SourceSet sources(raw);
  auto writer = Regions::Open(cache, sources, "terrain-precision");
  auto exact = Regions::Open(cache, sources, "terrain-precision");
  auto preview = Regions::Open(cache, sources, "terrain-precision");
  CHECK(writer && exact && preview, "region services open the same asset store");
  if (!writer || !exact || !preview) { return Report(); }

  GroundRegionAsset region;
  const auto surface = region.Surfaces.addSurface("ground", Material{});
  if (!surface) { return Report(); }
  region.GroundSurface = surface->index();
  region.Terrain.Sheets = {Hills({.Zoom = 8, .X = 128, .Y = 128}, 200),
                           Hills({.Zoom = 8, .X = 146, .Y = 128}, 224)};
  region.Terrain.Tiles = region.Terrain.Sheets.size();
  const std::string key(64, '9');
  const auto saved = StoreGroundRegion(
      **writer, key, GroundRegionBoundsEcef(region), region, Generators::WaterAsset{});
  CHECK(saved, "terrain with two different height bases can be persisted");
  if (!saved) { return Report(); }

  const auto nearAt = cache->PackageBytesRead();
  const auto near = Restore(**exact, key, [](const Box &) { return 0.0; });
  const auto nearBytes = cache->PackageBytesRead() - nearAt;
  CHECK(near && near->CompactTerrainPacks == 0 &&
            std::ranges::equal(near->Region.Terrain.Sheets, region.Terrain.Sheets, SameWords),
        "exact requests reconstruct original float words, including equal residual blobs");

  const auto farAt = cache->PackageBytesRead();
  const auto far = Restore(**preview, key, [](const Box &) { return 0.0625; });
  const auto farBytes = cache->PackageBytesRead() - farAt;
  CHECK(far && far->CompactTerrainPacks == 2 && farBytes < nearBytes &&
            far->Region.Terrain.Sheets.size() == region.Terrain.Sheets.size(),
        "certified far terrain covers both tiles while reading no residual bodies");
  if (far) {
    for (size_t i = 0; i < region.Terrain.Sheets.size(); ++i) {
      const auto &rounded = far->Region.Terrain.Sheets[i];
      CHECK(rounded.Tile == region.Terrain.Sheets[i].Tile &&
                std::ranges::equal(rounded.Samples(),
                                   region.Terrain.Sheets[i].Samples(),
                                   [](float a, float b) { return std::abs(a - b) <= 0.0625f; }),
            "the accepted compact package preserves address and stays within its height bound");
    }
  }
  return Report();
}
