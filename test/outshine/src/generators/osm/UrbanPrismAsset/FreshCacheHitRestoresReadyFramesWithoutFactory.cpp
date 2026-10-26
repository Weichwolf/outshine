#include "Check.h"
#include "UrbanPrismAsset.h"
#include "Sha256.h"
#include <PrismGrid.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
namespace Osm = Generators::Osm;
constexpr float kGeometryTopM = 20.0F;
constexpr double kFixtureEcefX = 6378137.0;
constexpr std::string_view kPublishedSurface = "published surface";

Osm::UrbanPrisms Product() {
  Osm::UrbanPrisms product;
  product.Geometry.Samples = std::make_shared<const std::vector<PrismSpan>>(
      std::vector<PrismSpan>{{.Lower = -10, .Upper = kGeometryTopM}, {.Lower = 5, .Upper = 5}});
  product.Geometry.Patches = {{.Columns = 2, .Rows = 1, .PitchX = 4, .PitchZ = 6}};
  product.Placements.resize(1);
  auto &frame = product.Placements.front().EcefFromLocal;
  frame.At(0, 0) = 0;
  frame.At(1, 0) = 1;
  frame.At(0, 1) = 1;
  frame.At(1, 1) = 0;
  frame.At(2, 2) = -1;
  frame.At(0, 3) = kFixtureEcefX;
  return product;
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-prism-assets-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;
  std::filesystem::create_directories(root, error);
  CHECK(!error, "the isolated native cache directory opens");
  const auto path = (root / "assets.sqlite").string();
  const std::array<Osm::UrbanPatchRange, 1> patches{{{.Columns = 2, .Rows = 1}}};
  const auto source = Sha256Hex("native area source content");
  auto terrain = Sha256Digest(kPublishedSurface.data(), kPublishedSurface.size());
  const auto identity = Osm::UrbanPrismRequestKey(source, terrain, patches, 3);
  CHECK(identity, "the ready asset request binds source, surface, selection and level");
  if (!identity) { return Report(); }
  const auto &request = *identity;
  size_t generations = 0;
  {
    auto cache = AssetCache::Open(path);
    CHECK(cache, "a native asset store opens");
    if (!cache) { return Report(); }
    const auto first =
        ResolveAssetRequest(**cache, request, Osm::kUrbanPrismBytesMost, [&](size_t most) {
          ++generations;
          return Osm::PackageUrbanPrisms(Product(), request, {.Level = 3, .BytesMost = most});
        });
    CHECK(first && generations == 1, "a miss packages and persists a complete ready product once");
  }
  {
    auto cache = AssetCache::Open(path);
    CHECK(cache, "a new owner reopens the persisted asset database");
    if (!cache) { return Report(); }
    const auto hit = ResolveAssetRequest(**cache, request, Osm::kUrbanPrismBytesMost, {});
    CHECK(hit && generations == 1,
          "a source-free cache hit never calls a factory or height sampler");
    if (hit) {
      const auto restored = Osm::DecodeUrbanPrisms(hit->Bytes());
      CHECK(restored && restored->Placements.front().EcefFromLocal.At(0, 3) == kFixtureEcefX &&
                restored->Geometry.Samples->front().Lower == -10 &&
                restored->Geometry.Samples->front().Upper == kGeometryTopM,
            "the fresh cached product already contains placed native geometry");
    }
    const auto selected =
        (*cache)->Select({.Bounds = {.Min = {{6378127, 0, -6}}, .Max = {{6378157, 4, 0}}},
                          .Kind = "urban-prisms",
                          .Level = 3,
                          .Radius = std::nullopt,
                          .Planes = {}});
    CHECK(selected && selected->size() == 1 && !selected->front().RequestKey && hit &&
              selected->front().Key == hit->Record().Key && hit->Record().RequestKey == request,
          "ready geometry has content-only spatial metadata and request-specific lookup context");
    terrain[0] ^= 1;
    const auto changed = Osm::UrbanPrismRequestKey(source, terrain, patches, 3);
    CHECK(changed && !ResolveAssetRequest(**cache, *changed, Osm::kUrbanPrismBytesMost, {}),
          "changed published terrain misses rather than loading an unrelated ready asset");
  }
  CHECK(!Osm::PackageUrbanPrisms(Product(), "unknown-request", {.Level = 3}),
        "unbound source identity cannot publish a ready product");
  CHECK(!Osm::PackageUrbanPrisms({}, request, {.Level = 3}),
        "no selected demand does not create a meaningless origin asset");
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the private native cache is removed after qualification");
  return Report();
}
