#include "OpenAssetStore.h"
#include "StoreGroundRegion.h"
#include "PreparedGroundRegions.h"
#include "BinaryValueArchive.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t kLimit = size_t{1024} * 1024;
  const auto directory = PlantedPath("composed-ground-package");
  std::filesystem::remove_all(directory);
  const auto store = OpenAssetStore(directory);
  Data::RawSourceCache sources({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  const Data::SourceSet inputs(sources);
  const auto regions = Generators::Osm::PreparedGroundRegions::Open(store, inputs, "codec-test");
  CHECK(regions, "the producer uses the existing shared store");
  if (!regions) { return Report(); }
  GroundRegionAsset region;
  const auto surface = region.Surfaces.addSurface("ground", Material{});
  CHECK(surface, "the region owns a native material");
  if (!surface) { return Report(); }
  region.GroundSurface = surface->index();
  const Generators::WaterAsset water;
  const auto groundBytes = EncodeGroundRegionAsset(region, kLimit);
  const auto waterBytes = water.EncodeNative(kLimit);
  CHECK(groundBytes && waterBytes, "independent native products encode");
  if (!groundBytes || !waterBytes) { return Report(); }
  BinaryValueWriter expected(kLimit);
  CHECK(expected(uint64_t{0x35524e474fULL}, static_cast<uint64_t>(groundBytes->size())) &&
            expected.Out.Put(*groundBytes) && expected(static_cast<uint64_t>(waterBytes->size())) &&
            expected.Out.Put(*waterBytes),
        "the original package format composes independent length-prefixed products");
  const std::string key(64, 'a');
  const auto saved =
      StoreGroundRegion(**regions, key, GroundRegionBoundsEcef(region), region, water);
  CHECK(saved, "the composed producer publishes through the existing store");
  const auto cached = store->Load(key, kLimit);
  CHECK(cached && *cached && std::ranges::equal((**cached).Bytes(), expected.Out.Bytes()),
        "direct composition preserves every existing package byte");
  BinaryValueWriter exact(waterBytes->size() + 1);
  CHECK(exact(uint8_t{7}) && water.EncodeNative(exact) &&
            std::ranges::equal(exact.Out.Bytes().subspan(1), *waterBytes),
        "water uses the caller's exact remaining budget without replacing its prefix");
  BinaryValueWriter shortOutput(waterBytes->size());
  CHECK(shortOutput(uint8_t{7}) && !water.EncodeNative(shortOutput),
        "water refuses an outer packet one byte below its required budget");
  std::filesystem::remove_all(directory);
  return Report();
}
