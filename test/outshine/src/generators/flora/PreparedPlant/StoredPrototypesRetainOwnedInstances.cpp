#include "PreparedPlantFixture.h"
#include "PreparedPlant.h"
#include "Check.h"
#include <cstddef>
#include <expected>
#include <system_error>
#include "content/AssetGeneration.h"
#include "Sha256.h"
#include <algorithm>
#include <filesystem>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kBudget = 16384;

void SameProduct(const PreparedPlant &source, const PreparedPlant &loaded) {
  CHECK(loaded.PlanIdentity == source.PlanIdentity && loaded.Root == source.Root,
        "prepared product retains its canonical plan identity and requested root");
  CHECK(loaded.Axes.size() == 2 && loaded.Axes[1].Id == kPlantBranch &&
            loaded.Axes[1].FirstIndex == 3 && loaded.Axes[1].IndexCount == 3,
        "wood index ranges retain source-axis identity");
  CHECK(loaded.Prototypes.parts() == 3 && loaded.Leaves.size() == 2 &&
            loaded.Leaves[0].Variant == 2 && loaded.Leaves[1].Variant == 7 &&
            loaded.Leaves[0].Ids == source.Leaves[0].Ids &&
            loaded.Leaves[0].Axes == source.Leaves[0].Axes,
        "three leaves share two prototypes without renumbering sparse variants or owners");
  for (int part = 0; part < source.Prototypes.parts(); ++part) {
    CHECK(std::ranges::equal(source.Prototypes.positionsOf(part),
                             loaded.Prototypes.positionsOf(part)) &&
              std::ranges::equal(source.Prototypes.normalsOf(part),
                                 loaded.Prototypes.normalsOf(part)) &&
              std::ranges::equal(source.Prototypes.textureOf(part),
                                 loaded.Prototypes.textureOf(part)) &&
              std::ranges::equal(source.Prototypes.trianglesOf(part),
                                 loaded.Prototypes.trianglesOf(part)),
          "native geometry streams survive the product boundary unchanged");
  }
  for (size_t group = 0; group < source.Leaves.size(); ++group) {
    for (size_t leaf = 0; leaf < source.Leaves[group].Ids.size(); ++leaf) {
      CHECK(source.Leaves[group].Transforms[leaf].Column ==
                loaded.Leaves[group].Transforms[leaf].Column,
            "native placement survives without rebuilding frames or positions");
    }
  }
}
}

int main() {
  const auto source = PreparedPlantFixture();
  const auto encoded = EncodePreparedPlant(source, kBudget);
  CHECK(encoded, "complete prepared product encodes");
  if (!encoded) { return Report(); }
  const std::string key = Sha256Hex(encoded->data(), encoded->size());
  const std::string path = PlantedPath("prepared-plant.sqlite");
  std::error_code error;
  std::filesystem::remove(path, error);
  {
    auto cache = AssetCache::Open(path);
    CHECK(cache, "shared asset cache opens for the cold request");
    if (!cache) { return Report(); }
    size_t builds = 0;
    const AssetFactory generate =
        [&](size_t budget) -> std::expected<GeneratedAssetPackage, std::string> {
      ++builds;
      if (encoded->size() > budget) { return std::unexpected("budget"); }
      return GeneratedAssetPackage{.Records = {{.Key = key,
                                                .Kind = "plant-prototype",
                                                .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                                .ByteCount = encoded->size()}},
                                   .Bytes = *encoded};
    };
    const auto built = ResolveAsset(**cache, key, kBudget, generate);
    CHECK(built && builds == 1, "only the missing product invokes its producer");
  }
  {
    auto cache = AssetCache::Open(path);
    CHECK(cache, "fresh cache connection opens the persisted product");
    if (!cache) { return Report(); }
    const auto hit = ResolveAsset(**cache, key, kBudget, {});
    CHECK(hit, "persisted product loads with no generator or plan available");
    if (hit) {
      const auto loaded = DecodePreparedPlant(hit->Bytes(), kBudget);
      CHECK(loaded, "stored product decodes without source preparation or meshing");
      if (loaded) { SameProduct(source, *loaded); }
    }
  }
  std::filesystem::remove(path, error);
  return Report();
}
