#include "Check.h"
#include "content/AssetGeneration.h"
#include "content/GeometryAsset.h"
#include "generation/Generate.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

class Ground final : public Generators::HeightSampler {
public:
  mutable size_t Calls = 0;

  std::optional<double> sampleHeightAslM(const LongitudeLatitudeHeight &) const override {
    ++Calls;
    return 42;
  }
};

class CachedTriangle final : public Generators::Generator {
public:
  explicit CachedTriangle(AssetCache &cache) : Cache_(cache) {}

  std::string_view kind() const override { return "external-triangle"; }

  Product make(const Generators::Request &asked) const override {
    const std::string key(64, asked.Seed == 0 ? 'a' : 'b');
    const auto asset = ResolveAsset(Cache_, key, kBytes, [&](size_t) {
      if (!asked.Ground) {
        return std::expected<GeneratedAssetPackage, std::string>(
            std::unexpected("ground unavailable"));
      }
      const auto height = asked.Ground->sampleHeightAslM({});
      if (!height) {
        return std::expected<GeneratedAssetPackage, std::string>(std::unexpected("ground unknown"));
      }
      const auto y = static_cast<float>(*height);
      const std::array<float, 9> positions{0, y, 0, 0, y, 1, 1, y, 0};
      const std::array<uint32_t, 3> triangles{0, 1, 2};
      Geometry geometry;
      const auto part = geometry.addPart("triangle", {});
      if (!part || !geometry.setPositions(*part, positions) ||
          !geometry.setTriangles(*part, triangles)) {
        return std::expected<GeneratedAssetPackage, std::string>(
            std::unexpected("invalid generated geometry"));
      }
      auto encoded = EncodeGeometryAsset(geometry, kBytes);
      if (!encoded) {
        return std::expected<GeneratedAssetPackage, std::string>(
            std::unexpected("native geometry exceeds its package budget"));
      }
      GeneratedAssetPackage package;
      package.Bytes = std::move(*encoded);
      package.Records.push_back({.Key = key,
                                 .Kind = "external-triangle",
                                 .Bounds = {.Min = {{0, *height, 0}}, .Max = {{1, *height, 1}}},
                                 .ByteCount = package.Bytes.size()});
      return std::expected<GeneratedAssetPackage, std::string>(std::move(package));
    });
    if (!asset) { return std::unexpected(asset.error()); }
    auto geometry = DecodeGeometryAsset(asset->Bytes(), kBytes);
    if (!geometry) { return std::unexpected("invalid cached native geometry"); }
    return std::move(*geometry);
  }

private:
  static constexpr size_t kBytes = 4096;
  AssetCache &Cache_;
};
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-public-assets-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto path = (root / "assets.sqlite").string();
  Ground ground;
  {
    auto cache = AssetCache::Open(path);
    CHECK(cache.has_value(), "public native cache opens");
    if (!cache) { return Report(); }
    CachedTriangle generator(**cache);
    const auto cold = generator.make({.Ground = &ground});
    CHECK(cold && cold->positionsOf(0)[1] == 42 && ground.Calls == 1,
          "external generator prepares and persists complete native geometry on a miss");
  }
  {
    auto cache = AssetCache::Open(path);
    CHECK(cache.has_value(), "stored native products survive closing their owner");
    if (!cache) { return Report(); }
    CachedTriangle generator(**cache);
    const auto hit = generator.make({});
    CHECK(hit && hit->parts() == 1 && hit->trianglesOf(0).size() == 3 &&
              hit->positionsOf(0)[1] == 42 && ground.Calls == 1,
          "fresh offline generator loads geometry without any provider or generation");
    const auto found = (*cache)->Select(
        {.Bounds = {.Min = {{-1, 40, -1}}, .Max = {{2, 43, 2}}}, .Kind = generator.kind()});
    CHECK(found && found->size() == 1 && found->front().Key == std::string(64, 'a'),
          "external native products participate in the common spatial index");
    CHECK(!generator.make({.Seed = 1}) && !(*cache)->Find(std::string(64, 'b'))->has_value(),
          "changed identity misses and unavailable inputs cannot publish partial products");
  }
  std::filesystem::remove_all(root);
  return Report();
}
