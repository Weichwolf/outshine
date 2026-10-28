#include "BuildingImpostorSource.h"
#include "ImpostorViewJob.h"
#include "ImpostorFieldLoad.h"
#include "AssetPipeline.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "HeightField.h"
#include "Check.h"
#include "ReadTextFile.h"
#include "Sha256.h"
#include "PreparedStructureCodec.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <SDL3/SDL.h>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kBytes = size_t{1} * 1024 * 1024;
constexpr auto kDeadline = std::chrono::seconds(10);

Generators::RawTile Inputs() {
  Generators::RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 100}, raw.AnchorEcef);
  raw.LatLon = {48.001, 16.001, 48.001, 16.0012, 48.0012, 16.0012, 48.0012, 16.001};
  raw.Structures.push_back({.LocalFirst = 0,
                            .PointCount = 4,
                            .SourceFirst = 0,
                            .Cell = {.Index = 1, .Footprint = {}},
                            .HeightM = 30,
                            .Pitched = 0,
                            .WallColour = {},
                            .Facade = {}});
  return raw;
}

void Keys(const BuildingImpostorSource::Recipe &recipe) {
  CHECK(!recipe.Key().empty(), "native source/material/capture inputs have a content key");
  auto reordered = recipe;
  reordered.Cells = {1, 1};
  CHECK(reordered.Key() == recipe.Key(), "cell ordering and duplicates are not source variation");
  auto changed = recipe;
  changed.WallRoofMetreMaterials[0].Roughness = .5f;
  CHECK(changed.Key() != recipe.Key(), "actual PBR inputs are part of the raw-source identity");
  changed = recipe;
  changed.Frame.Origin[0] += 1;
  CHECK(changed.Key() != recipe.Key(),
        "a changed intrinsic frame cannot reuse another source field");
  changed = recipe;
  changed.Directions = {{{0, 0, 1}}};
  CHECK(changed.Key() != recipe.Key(), "the fixed capture catalogue changes content identity");
  changed = recipe;
  changed.WallRoofMetreMaterials[0].Alpha = AlphaMode::Blended;
  CHECK(changed.Key().empty(),
        "unsupported transmissive materials cannot enter the opaque source family");
}

AssetPublication Await(const AssetPipeline::RequestTicket &ticket) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    const auto result = ticket.Poll();
    if (result.Finished) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

void Resolve(const std::shared_ptr<AssetCache> &cache,
             const Content::ImpostorViewAssets &metadata,
             const std::shared_ptr<BuildingImpostorSource> &source,
             size_t expectedCalls) {
  Tasks workers(1);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = 4096,
                          .ReceiptBytesMost = 4096});
  auto consumer = pipeline.OpenConsumer({.Identity = cache});
  CHECK(consumer.has_value(), "the native field consumer opens");
  if (!consumer) { return; }
  std::atomic<size_t> calls{0};
  const auto key = metadata.FieldKey({0});
  ImpostorViewJob job{.Cache = cache,
                      .Provenance = source->Provenance(),
                      .ProductKey = key,
                      .BytesMost = kBytes,
                      .Product = ImpostorViewJob::MaterialView{
                          .Selected = {0},
                          .Generate = [source, &calls](Content::ImpostorViewAssets::ViewIndex view,
                                                       size_t budget,
                                                       const std::stop_token &stop) {
                            ++calls;
                            return source->Generate(
                                view, budget, {.Samples = 1024, .SourceTests = 100000}, stop);
                          }}};
  const auto parsed = AssetProductKey::Parse(key);
  if (!parsed) {
    CHECK(false, "native metadata has a valid field key");
    return;
  }
  auto ticket = pipeline.RequestResolved(*consumer,
                                         {.Key = *parsed,
                                          .Generation = consumer->Generation(),
                                          .InputBytes = 256,
                                          .ConsumerBytes = 0,
                                          .ReceiptBytesMost = 4096},
                                         std::move(job));
  CHECK(ticket.has_value(), "the typed native source job admits");
  if (!ticket) { return; }
  const auto ready = Await(*ticket);
  CHECK(ready.Finished && ready.Status == AssetPublicationStatus::Ready, ready.Message.c_str());
  CHECK(calls == expectedCalls, "only a missing field enters the native building producer");
  if (ready.Status != AssetPublicationStatus::Ready) { return; }
  auto load = ImpostorFieldLoad::Start(
      workers, cache, source->Provenance(), {0}, kBytes, std::move(*ticket));
  CHECK(load.has_value(), "the stored native field enters the CPU-ready phase");
  if (!load) { return; }
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    const auto field = load->Poll();
    if (!field) {
      CHECK(false, field.error().c_str());
      return;
    }
    if (*field) {
      CHECK((**field).Pixels() == 32 && (**field).Views().size() == 1,
            "the complete native source result is ready for its World consumer");
      CHECK(std::ranges::any_of((**field).Views()[0].Texels,
                                [](const auto &pixel) { return pixel.Surface != 0; }),
            "the produced field contains the original building");
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  CHECK(false, "native field loading terminates within the offline deadline");
}

void Run(const std::filesystem::path &root) {
  Data::RawSourceCache store({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(store);
  auto opened = Generators::PreparedBuildingAssets::Open((root / "buildings").string(), sources);
  CHECK(opened.has_value(), "the real native building asset store opens");
  if (!opened) { return; }
  const auto &assets = *opened;
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100, 100, 100, 100};
  const auto heights = Ground::HeightField::Of(0, {block});
  if (!heights) {
    CHECK(false, "fixture heights exist");
    return;
  }
  const auto raw = Inputs();
  const auto key = assets->Key({.Zoom = 0, .X = 0, .Y = 0}, 0, {}, 1000);
  const std::atomic_bool stop{false};
  auto base = assets->Generate(key, raw, *heights, stop);
  CHECK(base && *base, "native game assets build without the archive adapter");
  if (!base || !*base) { return; }
  const BuildingImpostorSource::Recipe recipe{
      .BaseKey = key,
      .Cells = {1},
      .Frame = {.Axes = EnuAxesEcef({.LongitudeDeg = 16, .LatitudeDeg = 48}),
                .Origin = {{3, 4, 5}}},
      .Directions = {{{0, 0, -1}}},
      .WallRoofMetreMaterials = {},
      .Pixels = 32};
  Keys(recipe);
  auto database = AssetCache::Open((root / "views.sqlite").string());
  if (!database) {
    CHECK(false, "the raw-view store opens");
    return;
  }
  const std::shared_ptr<AssetCache> cache(std::move(*database));
  const auto source = std::make_shared<BuildingImpostorSource>(recipe, assets);
  const auto before = assets->Costs();
  const auto refused = source->Generate({0}, kBytes, {.Samples = 1023, .SourceTests = 100000});
  CHECK(!refused && assets->Costs().ReadBytes == before.ReadBytes &&
            assets->Costs().SurfaceReadBytes == before.SurfaceReadBytes,
        "sample admission precedes native descriptor and form IO");
  auto metadata = source->ResolveMetadata(*cache);
  CHECK(metadata.has_value(), metadata ? "source metadata resolves" : metadata.error().c_str());
  CHECK(assets->Costs().SurfaceReadBytes == before.SurfaceReadBytes,
        "native source selection publishes metadata before form reads");
  if (!metadata) { return; }
  const auto &world = metadata->Metadata().WorldFromField;
  const Vec3 local{{2.5, 7, 11.5}};
  const Vec3 expected = raw.AnchorEcef + recipe.Frame.Origin + recipe.Frame.SourceDirection(local);
  CHECK(world && Length(world->TransformPoint(local) - expected) < 1e-9,
        "native metadata maps captured local coordinates back to the prepared World frame");
  Resolve(cache, *metadata, source, 1);
  CHECK(assets->Costs().SurfaceReadBytes > before.SurfaceReadBytes,
        "the field miss loads its native source forms");
  const auto after = assets->Costs();
  const auto offline = std::make_shared<BuildingImpostorSource>(recipe, nullptr);
  auto warm = offline->ResolveMetadata(*cache);
  CHECK(warm.has_value(), "a new source owner resolves metadata with no source store");
  CHECK(warm && warm->Metadata().WorldFromField && world &&
            warm->Metadata().WorldFromField->Column == world->Column &&
            Length(warm->Metadata().WorldFromField->TransformPoint(local) - expected) < 1e-9,
        "source-free warm placement preserves the complete intrinsic World frame");
  if (warm) { Resolve(cache, *warm, offline, 0); }
  CHECK(assets->Costs().ReadBytes == after.ReadBytes &&
            assets->Costs().SurfaceReadBytes == after.SurfaceReadBytes,
        "stored fields repeat no native source decode or form generation");
  if (warm) {
    auto unframed = warm->Metadata();
    unframed.WorldFromField.reset();
    const Vec3 padding{{1000, 1000, 1000}};
    CHECK(Content::ImpostorViewAssets::Publish(
              *cache,
              source->Provenance(),
              std::move(unframed),
              {.Min = raw.AnchorEcef - padding, .Max = raw.AnchorEcef + padding}),
          "an unframed index supplies the independent stale-source counterexample");
    CHECK(!offline->ResolveMetadata(*cache),
          "an unframed native hit cannot invent a World placement without its source");
    const auto repaired = source->ResolveMetadata(*cache);
    CHECK(repaired && repaired->Metadata().WorldFromField && world &&
              repaired->Metadata().WorldFromField->Column == world->Column,
          "the retained native source can repair a missing intrinsic World frame");
  }
}

void Partial(const std::filesystem::path &root, std::string_view index) {
  const auto bytes = ReadTextFile(index, size_t{64} * 1024 * 1024);
  CHECK(bytes.has_value(), "the explicitly requested original index is readable");
  if (!bytes) { return; }
  Data::RawSourceCache store({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(store);
  auto assets = Generators::PreparedBuildingAssets::Open((root / "buildings").string(), sources);
  auto sourceStore = AssetCache::Open((root / "buildings/assets.sqlite").string());
  auto cache = AssetCache::Open((root / "views.sqlite").string());
  if (!assets || !sourceStore || !cache) {
    CHECK(false, "the isolated native stores open");
    return;
  }
  const std::span<const uint8_t> data(reinterpret_cast<const uint8_t *>(bytes->data()),
                                      bytes->size());
  const auto base = Generators::DecodePreparedStructureIndex(data, size_t{128} * 1024 * 1024);
  CHECK(base.has_value(), "the original index is a valid native prepared product");
  if (!base) { return; }
  Box bounds;
  for (const auto &surface : base->Surfaces) {
    if (surface.Bounds().Empty()) { continue; }
    for (unsigned corner = 0; corner < 8; ++corner) {
      bounds.Cover(base->AnchorEcef + surface.Bounds().Corner(corner));
    }
  }
  const auto key = Sha256Hex(bytes->data(), bytes->size());
  const AssetRecord record{.Key = key,
                           .Kind = "buildings",
                           .Bounds = bounds,
                           .Package = {},
                           .ByteCount = data.size(),
                           .Parent = {}};
  CHECK((*sourceStore)->Publish(std::span(&record, 1), data).has_value(),
        "the original headers enter the native store without form packages");
  BuildingImpostorSource::Recipe recipe{
      .BaseKey = key,
      .Cells = {},
      .Frame = {.Axes = {.East = {{1, 0, 0}}, .North = {{0, 1, 0}}, .Up = {{0, 0, 1}}},
                .Origin = {}},
      .Directions = {{{0, 0, 1}}},
      .WallRoofMetreMaterials = {},
      .Pixels = 32};
  for (uint32_t cell = 1; cell <= 64; ++cell) { recipe.Cells.push_back(cell); }
  BuildingImpostorSource source(std::move(recipe), *assets);
  const auto metadata = source.ResolveMetadata(**cache);
  CHECK(!metadata && metadata.error().contains("unsupported building source ordinal"),
        "real unresolved originals prevent a complete-source metadata declaration");
  CHECK((*assets)->Costs().SurfaceReadBytes == 0 && (*assets)->Costs().SurfaceWrites == 0,
        "incomplete source rejection requires no form loading or generation");
  const auto absent = Content::ImpostorViewAssets::Open(**cache, source.Provenance());
  CHECK(absent && !*absent, "an incomplete original set publishes no replacement metadata");
}

}

int main(int argc, char *const *argv) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    CHECK(false, SDL_GetError());
    return Report();
  }
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-native-building-views-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  if (argc == 2) {
    Partial(root, argv[1]);
  } else {
    Run(root);
  }
  std::filesystem::remove_all(root);
  SDL_Quit();
  return Report();
}
