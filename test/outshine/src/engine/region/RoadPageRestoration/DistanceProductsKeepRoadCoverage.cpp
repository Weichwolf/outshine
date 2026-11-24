#include "Check.h"
#include "OpenAssetStore.h"
#include "RoadPageMeshProducer.h"
#include "RoadPageRestoration.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kBytesMost = size_t{1024} * 1024;

std::optional<AssetRecord> Bake(PreparedRoadPages &pages,
                                RoadPageAddress address,
                                std::optional<RoadDisplayLimits> limits = std::nullopt) {
  RoadPageRecipe recipe;
  recipe.Intervals.push_back({.Edge = address.Parts[0].Index,
                              .Lane = 3,
                              .Stations = {{.EastM = 0}, {.EastM = 1024}},
                              .Surface = {.HalfWidthM = 3,
                                          .Profile = RoadProfile::Simple,
                                          .WearsLinear = {{.2f, .3f, .4f}},
                                          .Form = RibbonForm::Surface}});
  const RoadPageMeshProducer producer(RoadPageKey(address), std::move(recipe));
  auto record = pages.Prepare({.Address = std::move(address),
                               .Parent = {},
                               .RequestKey = {},
                               .NativeBytesMost = kBytesMost,
                               .SweepLimits = limits},
                              &producer);
  return record ? std::optional(std::move(*record)) : std::nullopt;
}

void Restore(const std::shared_ptr<PreparedRoadPages> &pages,
             const std::shared_ptr<RoadPageManifest> &manifest,
             const Vec3 &eye,
             double travelM,
             const std::vector<std::string> &keys,
             size_t displayPages,
             std::optional<Vec3> pathEnd = std::nullopt) {
  Tasks pool(1);
  const auto restoration = RoadPageRestoration::Begin(
      pool,
      pages,
      manifest,
      std::nullopt,
      {},
      std::nullopt,
      RoadPageRestoration::DefaultLimits,
      {},
      RoadPageRestoration::DisplayView{.Eye = eye,
                                       .ViewportPx = {{1280, 720}},
                                       .Projection = {.FocalPx = 600, .AllowedErrorPx = 1},
                                       .TranslationM = travelM,
                                       .PathEnd = pathEnd});
  CHECK(restoration, "the same manifest supports different camera travel domains");
  if (!restoration) { return; }
  std::vector<std::string> seen;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    const auto done =
        (*restoration)
            ->Advance(
                [&](const RoadPageAsset &asset,
                    const RoadPageReference &reference,
                    std::chrono::steady_clock::time_point) -> std::expected<bool, std::string> {
                  seen.push_back(reference.Key);
                  CHECK(seen.size() <= manifest->Pages.size() &&
                            reference.Parts == manifest->Pages[seen.size() - 1].Parts &&
                            asset.Address == StoredRoadPageAddress(*manifest, reference),
                        "even a replacement carries the original logical source coverage");
                  return true;
                });
    CHECK(done && !(*restoration)->Missing(), "a display miss retains the original road page");
    if (!done || *done || (*restoration)->Missing()) { break; }
    const auto barrier = pool.Post([] {});
    if (barrier == Tasks::kNoTask || !pool.AwaitCompletion(barrier, 1) ||
        !pool.TakeCompletion(barrier)) {
      break;
    }
  }
  CHECK(seen == keys && (*restoration)->DisplayPages() == displayPages,
        "the cached detail matches the whole camera travel domain");
}

void CheckDistanceProducts(const std::filesystem::path &directory) {
  const auto cache = OpenAssetStore(directory);
  auto opened = PreparedRoadPages::Open(cache);
  CHECK(opened, "the road asset database is available");
  if (!opened) { return; }
  const std::shared_ptr<PreparedRoadPages> pages = std::move(*opened);
  const auto manifest = std::make_shared<RoadPageManifest>();
  manifest->PlanKey.assign(64, 'a');
  manifest->MaterialKey.assign(64, 'b');
  if (!manifest->Materials.addSurface("streets", Material{})) { return; }
  for (uint32_t index = 0; index < 2; ++index) {
    RoadPageReference original{.Key = {},
                               .Bounds = {},
                               .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = index}},
                               .Clustered = true,
                               .ShapeKey = {},
                               .Sources = {}};
    const auto record = Bake(*pages, StoredRoadPageAddress(*manifest, original));
    CHECK(record, "a canonical road page renders at ground level");
    if (!record) { return; }
    original.Key = record->Key;
    original.Bounds = record->Bounds;
    original.ByteCount = record->ByteCount;
    manifest->Pages.push_back(std::move(original));
  }
  const auto &first = manifest->Pages[0];
  const auto original = StoredRoadPageAddress(*manifest, first);
  auto variant = original;
  variant.Level = 1;
  variant.PositionErrorM = 1.0;
  variant.DisplayPolicy = RoadDisplayPolicy::StoredSurfaceEnvelope;
  const auto coarse = Bake(*pages, variant, RoadDisplayLimits{128, 256});
  CHECK(coarse && coarse->ByteCount < first.ByteCount,
        "the generated display package is smaller than its original");
  if (!coarse) { return; }
  const auto removed = cache->Remove(first.Key);
  CHECK(removed, "the far road remains available after its original payload is evicted");
  const Vec3 far = first.Bounds.Min + Vec3{{0, 0, 5000}};
  Restore(pages, manifest, far, 0, {coarse->Key, manifest->Pages[1].Key}, 1);
  Restore(
      pages, manifest, far, 0, {coarse->Key, manifest->Pages[1].Key}, 1, far + Vec3{{6000, 0, 0}});
  CHECK(Bake(*pages, original), "the fine road can be made ready again");
  Restore(pages, manifest, far, 5000, {first.Key, manifest->Pages[1].Key}, 0);
  Restore(pages, manifest, far, 0, {first.Key, manifest->Pages[1].Key}, 0, first.Bounds.Middle());
}
}

int main() {
  const auto directory = std::filesystem::temp_directory_path() /
                         ("outshine-road-distance-" + std::to_string(getpid()));
  std::error_code error;
  std::filesystem::create_directory(directory, error);
  if (error) { return 1; }
  CheckDistanceProducts(directory);
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the isolated road assets are released");
  return Report();
}
