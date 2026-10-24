#include "Earth.h"
#include "ControlledRoadPageProducer.h"
#include <stop_token>
#include "PreparedRoadPages.h"
#include "Sha256.h"
#include "Check.h"
#include <AssetCache.h>
#include <GeometryAsset.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr size_t kLimit = 4096;

constexpr LongitudeLatitude kAnchor{.LongitudeDeg = 8, .LatitudeDeg = 49};

std::optional<RoadPageManifest> Prepare(PreparedRoadPages &pages) {
  RoadPageManifest manifest;
  manifest.PlanKey.assign(64, 'a');
  manifest.Anchor = kAnchor;
  if (!manifest.Materials.addSurface("streets", Material{})) { return std::nullopt; }
  const auto material = EncodeGeometryAsset(manifest.Materials, kLimit);
  if (!material) { return std::nullopt; }
  manifest.MaterialKey = Sha256Hex(material->data(), material->size());
  for (uint32_t index = 0; index < 2; ++index) {
    const RoadPageAddress address{.PlanKey = manifest.PlanKey,
                                  .MaterialKey = manifest.MaterialKey,
                                  .MaterialIndex = 0,
                                  .Anchor = manifest.Anchor,
                                  .Level = 0,
                                  .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = index}}};
    auto result = pages.Resolve(
        {.Address = address, .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit},
        outshine::Test::RoadProducer([index](const RoadPageRequest &, const std::stop_token &) {
          const auto x = static_cast<float>(index);
          RoadPageGeometry page;
          page.Mesh.PositionM = {x, 0, 0, x + 1, 0, 0, x, 1, 0};
          page.Mesh.NormalM = {0, 0, 1, 0, 0, 1, 0, 0, 1};
          page.Mesh.ColourRgba = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
          page.Mesh.Index = {0, 1, 2};
          page.Parts = {{.Kind = RoadPagePartKind::Interval, .Index = index}};
          return std::expected<RoadPageGeometry, std::string>(std::move(page));
        }).get());
    if (!result) { return std::nullopt; }
    manifest.Pages.push_back({.Key = result->Record.Key,
                              .Bounds = result->Record.Bounds,
                              .ByteCount = result->NativeBytes,
                              .Level = 0,
                              .Parts = address.Parts});
  }
  return manifest;
}

void CheckRemoval(PreparedRoadPages &pages,
                  AssetCache &cache,
                  const RoadPageManifest &manifest,
                  const std::string &key) {
  using namespace outshine::Test;
  CHECK(cache.Remove(manifest.Pages[1].Key).has_value(), "the unselected payload is evicted");
  const auto metadata = pages.LoadManifest(key);
  CHECK(metadata && *metadata &&
            EncodeRoadPageManifest(***metadata, PreparedRoadPages::ManifestBytesMost) ==
                EncodeRoadPageManifest(manifest, PreparedRoadPages::ManifestBytesMost),
        "complete manifest metadata survives an absent unselected payload");
  const auto selected = pages.LoadKey(manifest.Pages[0].Key, kLimit);
  CHECK(selected && *selected,
        "selected native payload remains independently readable after the unrelated eviction");
  CHECK(!pages.StoreManifest(manifest),
        "publication still requires all promised native dependencies to exist");
  const auto absent = pages.LoadKey(manifest.Pages[1].Key, kLimit);
  CHECK(absent && !*absent, "a needed missing payload remains an explicit ordinary miss");
  const auto retained = pages.LoadManifest(key);
  CHECK(retained && *retained,
        "metadata inspection and a direct payload miss do not invalidate unselected demand");
  CHECK(pages.InvalidateManifest(key).has_value(),
        "the selected-miss consumer retains its explicit invalidation hook");
  const auto invalidated = pages.LoadManifest(key);
  CHECK(invalidated && !*invalidated, "explicit invalidation removes the stale manifest hit");
}
}

int main() {
  using namespace outshine::Test;
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-tests.road-manifest-metadata-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "the private native asset directory is created");
  auto pages = PreparedRoadPages::Open(directory.string());
  CHECK(pages.has_value(), "the native road page service opens");
  const auto manifest = pages ? Prepare(**pages) : std::nullopt;
  CHECK(manifest.has_value(), "two independently stored native pages prepare shared metadata");
  const auto key = manifest
                       ? (*pages)->StoreManifest(*manifest)
                       : std::expected<std::string, std::string>(std::unexpected("no fixture"));
  CHECK(key.has_value(), "a complete manifest publishes before any eviction");
  auto cache = AssetCache::Open((directory / "assets.sqlite").string());
  CHECK(cache.has_value(), "the underlying native cache opens for a controlled eviction");
  if (key && cache && manifest) { CheckRemoval(**pages, **cache, *manifest, *key); }
  cache = std::unexpected(AssetCacheError::Storage);
  pages = std::unexpected("closed");
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the private fixture is removed after all cache handles close");
  return Report();
}
