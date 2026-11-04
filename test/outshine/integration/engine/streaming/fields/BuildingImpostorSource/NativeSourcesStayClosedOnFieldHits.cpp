#include "OpenAssetStore.h"
#include "ImpostorViewAssets.h"
#include "BuildingImpostorSource.h"
#include "BuildingMaterials.h"
#include "ImpostorViewJob.h"
#include "ImpostorFieldLoad.h"
#include "ImpostorMetadataRequest.h"
#include "AssetPipeline.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "HeightField.h"
#include "Check.h"
#include "ReadTextFile.h"
#include "Sha256.h"
#include "PreparedStructureCodec.h"
#include "PageIndexAsset.h"
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
#include <print>
#include <ratio>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
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

BuildingImpostorSource::Recipe Recipe(const std::string &baseKey) {
  return {.BaseKey = baseKey,
          .Cells = {1},
          .Sources = {},
          .Frame = {.Axes = EnuAxesEcef({.LongitudeDeg = 16, .LatitudeDeg = 48}),
                    .Origin = {{3, 4, 5}}},
          .Directions = {{{0, 0, -1}}},
          .WallRoofMetreMaterials = Generators::BuildingSurfaceMaterials(),
          .Pixels = 32};
}

void Keys(const BuildingImpostorSource::Recipe &recipe) {
  CHECK(!recipe.Key().empty(), "native source/material/capture inputs have a content key");
  auto reordered = recipe;
  reordered.Cells = {1, 1};
  CHECK(reordered.Key() == recipe.Key(), "cell ordering and duplicates are not source variation");
  auto group = recipe;
  group.Cells.clear();
  group.Sources = {1, 0, 1};
  const auto groupKey = group.Key();
  CHECK(!groupKey.empty() && groupKey != recipe.Key(),
        "source-group ordinals and cell ids occupy distinct cache identities");
  group.Sources = {0, 1};
  CHECK(group.Key() == groupKey, "group order and duplicates preserve canonical identity");
  group.Cells = {1};
  CHECK(group.Key().empty(), "a recipe cannot mix cells and explicit source groups");
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
  changed = recipe;
  changed.WallRoofMetreMaterials[0].Pattern = SurfacePattern::None;
  CHECK(!changed.Key().empty() && changed.Key() != recipe.Key(),
        "procedural capture inputs retain their pattern in the content identity");
  changed = recipe;
  changed.WallRoofMetreMaterials[0].Pattern = SurfacePattern::FacadeMetres;
  CHECK(!changed.Key().empty() && changed.Key() != recipe.Key(),
        "metre coordinates and normalized facade coordinates are different inputs");
  for (int variant = 0; variant < 6; ++variant) {
    changed = recipe;
    auto &material = changed.WallRoofMetreMaterials[0];
    switch (variant) {
      case 0: material.BaseColourMap.Image = 0; break;
      case 1: material.NormalMap.Image = 0; break;
      case 2: material.MetalRoughMap.Image = 0; break;
      case 3: material.Emission = {{0.1f, 0, 0}}; break;
      case 4: material.Unlit = true; break;
      case 5: material.Transmission = 0.5f; break;
      default: CHECK(false, "every negative fixture has an unsupported material input"); return;
    }
    CHECK(changed.Key().empty(), "unprovided images or unsupported light response are refused");
  }
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

std::optional<std::string> Resolve(const std::shared_ptr<AssetCache> &cache,
                                   const outshine::ImpostorViewAssets &metadata,
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
  if (!consumer) { return std::nullopt; }
  std::atomic<size_t> calls{0};
  const auto key = metadata.FieldKey({0});
  ImpostorViewJob job{
      .Cache = cache,
      .Provenance = source->Provenance(),
      .ProductKey = key,
      .BytesMost = kBytes,
      .Product = ImpostorViewJob::MaterialView{
          .Selected = {0},
          .Generate = [source, &calls](outshine::ImpostorViewAssets::ViewIndex view,
                                       size_t budget,
                                       const std::stop_token &stop) {
            ++calls;
            return source->Generate(
                view,
                budget,
                {.Samples = 1024, .Sources = 100, .Faces = 1000, .TriangleAttempts = 100000},
                stop);
          }}};
  const auto parsed = AssetProductKey::Parse(key);
  if (!parsed) {
    CHECK(false, "native metadata has a valid field key");
    return std::nullopt;
  }
  auto ticket = pipeline.RequestResolved(*consumer,
                                         {.Key = *parsed,
                                          .Generation = consumer->Generation(),
                                          .InputBytes = 256,
                                          .ConsumerBytes = 0,
                                          .ReceiptBytesMost = 4096},
                                         std::move(job));
  CHECK(ticket.has_value(), "the typed native source job admits");
  if (!ticket) { return std::nullopt; }
  const auto ready = Await(*ticket);
  CHECK(ready.Finished && ready.Status == AssetPublicationStatus::Ready, ready.Message.c_str());
  CHECK(calls == expectedCalls, "only a missing field enters the native building producer");
  if (ready.Status != AssetPublicationStatus::Ready) { return std::nullopt; }
  auto load = ImpostorFieldLoad::Start(
      workers, cache, source->Provenance(), {0}, kBytes, std::move(*ticket));
  CHECK(load.has_value(), "the stored native field enters the CPU-ready phase");
  if (!load) { return std::nullopt; }
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    const auto field = load->Poll();
    if (!field) {
      CHECK(false, field.error().c_str());
      return std::nullopt;
    }
    if (*field) {
      CHECK((**field).Pixels() == 32 && (**field).Views().size() == 1,
            "the complete native source result is ready for its World consumer");
      CHECK(std::ranges::any_of((**field).Views()[0].Texels,
                                [](const auto &pixel) { return pixel.Surface != 0; }),
            "the produced field contains the original building");
      CHECK(std::ranges::all_of(
                (**field).Surfaces(),
                [](const Material &material) { return material.Pattern == SurfacePattern::None; }),
            "captured materials never apply source facade patterns twice");
      CHECK((**field).Views()[0].Materials.size() == (**field).Views()[0].Texels.size(),
            "the field retains coupled material samples for every captured texel");
      bool patterned = false;
      const auto &view = (**field).Views()[0];
      for (size_t pixel = 0; pixel < view.Texels.size(); ++pixel) {
        const uint32_t surface = view.Texels[pixel].Surface;
        if (surface != 0 &&
            view.Materials[pixel].Roughness != (**field).Surfaces()[surface - 1].Roughness) {
          patterned = true;
        }
      }
      CHECK(patterned, "the captured field includes the actual procedural facade response");
      std::string error;
      const auto encoded = (**field).Encode(source->Provenance(), error);
      if (!encoded) {
        CHECK(false, error.c_str());
        return std::nullopt;
      }
      return Sha256Hex(encoded->data(), encoded->size());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  CHECK(false, "native field loading terminates within the offline deadline");
  return std::nullopt;
}

std::expected<outshine::ImpostorViewAssets, std::string>
Metadata(const std::shared_ptr<AssetCache> &cache,
         const std::shared_ptr<BuildingImpostorSource> &source) {
  Tasks workers(1);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = 32768,
                          .ReceiptBytesMost = 4096});
  auto consumer = pipeline.OpenConsumer({.Identity = cache});
  if (!consumer) { return std::unexpected("native metadata consumer unavailable"); }
  auto request = ImpostorMetadataRequest::Start(
      pipeline,
      *consumer,
      workers,
      cache,
      source->Provenance(),
      [cache, source](const std::stop_token &stop) {
        return source->ResolveMetadata(*cache, stop);
      },
      256);
  if (!request) { return std::unexpected("native metadata admission failed"); }
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    const auto ready = request->Poll();
    if (!ready) { return std::unexpected(ready.error()); }
    if (*ready) { return **ready; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return std::unexpected("native metadata request deadline expired");
}

int Offline(const std::filesystem::path &root,
            const std::string &baseKey,
            std::string_view expected) {
  auto opened = AssetCache::Open((root / "views.sqlite").string());
  CHECK(opened.has_value(), "fresh process opens only the saved field database");
  if (!opened) { return Report(); }
  const std::shared_ptr<AssetCache> cache(std::move(*opened));
  const auto source = std::make_shared<BuildingImpostorSource>(Recipe(baseKey), nullptr);
  const auto metadata = Metadata(cache, source);
  CHECK(metadata.has_value(), "fresh process restores metadata without source assets");
  if (metadata) {
    const auto restored = Resolve(cache, *metadata, source, 0);
    CHECK(restored && *restored == expected,
          "fresh process returns byte-identical original fields without producer calls");
  }
  return Report();
}

bool Restart(const std::string &executable,
             const std::filesystem::path &root,
             const std::string &baseKey,
             const std::string &digest) {
  const std::string directory = root.string();
  const std::array<const char *, 6> arguments{
      executable.c_str(), "--offline", directory.c_str(), baseKey.c_str(), digest.c_str(), nullptr};
  const std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process(
      SDL_CreateProcess(arguments.data(), false), SDL_DestroyProcess);
  if (!process) { return false; }
  int status = -1;
  return SDL_WaitProcess(process.get(), true, &status) && status == 0;
}

void Run(const std::filesystem::path &root, const std::string &executable) {
  Data::RawSourceCache store({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(store);
  auto opened = Generators::PreparedBuildingAssets::Open(
      outshine::Test::OpenAssetStore((root / "buildings").string()), sources);
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
  const Data::TileId tile{.Zoom = 0, .X = 0, .Y = 0};
  const std::string inputDigest(64, 'a');
  const auto key = assets->Key(tile, 0, {}, 1000, inputDigest);
  const auto request = assets->RequestKey(tile, {}, 1000, inputDigest);
  const std::atomic_bool stop{false};
  auto base = assets->Generate(key, raw, *heights, stop, request);
  CHECK(base && *base, "native game assets build without the archive adapter");
  if (!base || !*base) { return; }
  const auto beforeDemand = assets->Costs();
  const auto demand = assets->LoadBasisRequest(request);
  CHECK(demand && *demand && !(**demand).Bounds.Empty(),
        "the source receives its native address and bounds before loading original forms");
  if (!demand || !*demand) { return; }
  CHECK(assets->Costs().ReadBytes == beforeDemand.ReadBytes &&
            assets->Costs().SurfaceReadBytes == beforeDemand.SurfaceReadBytes &&
            assets->Costs().GeometryReadBytes == beforeDemand.GeometryReadBytes,
        "source demand bounds require only the existing basis and request metadata");
  const auto recipe = Recipe((**demand).BaseKey);
  Keys(recipe);
  auto database = AssetCache::Open((root / "views.sqlite").string());
  if (!database) {
    CHECK(false, "the raw-view store opens");
    return;
  }
  const std::shared_ptr<AssetCache> cache(std::move(*database));
  const auto source = std::make_shared<BuildingImpostorSource>(recipe, assets);
  const auto before = assets->Costs();
  const auto refused = source->Generate(
      {0}, kBytes, {.Samples = 1023, .Sources = 100, .Faces = 1000, .TriangleAttempts = 100000});
  CHECK(!refused && assets->Costs().ReadBytes == before.ReadBytes &&
            assets->Costs().SurfaceReadBytes == before.SurfaceReadBytes,
        "sample admission precedes native descriptor and form IO");
  auto metadata = Metadata(cache, source);
  CHECK(metadata.has_value(), metadata ? "source metadata resolves" : metadata.error().c_str());
  CHECK(assets->Costs().SurfaceReadBytes == before.SurfaceReadBytes,
        "native source selection publishes metadata before form reads");
  if (!metadata) { return; }
  const auto &world = metadata->Metadata().WorldFromField;
  const Vec3 local{{2.5, 7, 11.5}};
  const Vec3 expected = raw.AnchorEcef + recipe.Frame.Origin + recipe.Frame.SourceDirection(local);
  CHECK(world && Length(world->TransformPoint(local) - expected) < 1e-9,
        "native metadata maps captured local coordinates back to the prepared World frame");
  const auto coldField = Resolve(cache, *metadata, source, 1);
  CHECK(assets->Costs().SurfaceReadBytes > before.SurfaceReadBytes,
        "the field miss loads its native source forms");
  const auto after = assets->Costs();
  const auto offline = std::make_shared<BuildingImpostorSource>(recipe, nullptr);
  auto warm = Metadata(cache, offline);
  CHECK(warm.has_value(), "a new source owner resolves metadata with no source store");
  CHECK(warm && warm->Metadata().WorldFromField && world &&
            warm->Metadata().WorldFromField->Column == world->Column &&
            Length(warm->Metadata().WorldFromField->TransformPoint(local) - expected) < 1e-9,
        "source-free warm placement preserves the complete intrinsic World frame");
  if (warm) {
    const auto warmField = Resolve(cache, *warm, offline, 0);
    CHECK(coldField && warmField && *coldField == *warmField,
          "source-free cache hits retain exactly the captured procedural material field");
  }
  CHECK(coldField && Restart(executable, root, recipe.BaseKey, *coldField),
        "a genuinely fresh offline process needs only the stored native field");
  CHECK(assets->Costs().ReadBytes == after.ReadBytes &&
            assets->Costs().SurfaceReadBytes == after.SurfaceReadBytes,
        "stored fields repeat no native source decode or form generation");
  if (warm) {
    auto unframed = warm->Metadata();
    unframed.WorldFromField.reset();
    const Vec3 padding{{1000, 1000, 1000}};
    CHECK(outshine::ImpostorViewAssets::Publish(
              *cache,
              source->Provenance(),
              std::move(unframed),
              {.Min = raw.AnchorEcef - padding, .Max = raw.AnchorEcef + padding}),
          "an unframed index supplies the independent stale-source counterexample");
    CHECK(!Metadata(cache, offline),
          "an unframed native hit cannot invent a World placement without its source");
    const auto repaired = Metadata(cache, source);
    CHECK(repaired && repaired->Metadata().WorldFromField && world &&
              repaired->Metadata().WorldFromField->Column == world->Column,
          "the retained native source can repair a missing intrinsic World frame");
  }
}

void Groups(const std::filesystem::path &root) {
  Data::RawSourceCache store({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(store);
  auto assets = Generators::PreparedBuildingAssets::Open(
      outshine::Test::OpenAssetStore((root / "buildings").string()), sources);
  CHECK(assets, "the grouped native source store opens");
  if (!assets) { return; }
  auto raw = Inputs();
  const auto first = raw.LatLon;
  for (size_t point = 0; point < first.size(); point += 2) {
    raw.LatLon.push_back(first[point]);
    raw.LatLon.push_back(first[point + 1] + 0.002);
  }
  auto second = raw.Structures.front();
  second.LocalFirst = second.SourceFirst = 4;
  second.HeightM = 90;
  raw.Structures.push_back(second);
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100, 100, 100, 100};
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights, "the grouped source has constant terrain");
  if (!heights) { return; }
  const auto key = (*assets)->Key({.Zoom = 0, .X = 0, .Y = 0}, 0, {}, 1000, std::string(64, 'b'));
  const std::atomic_bool stop{false};
  const auto base = (*assets)->Generate(key, raw, *heights, stop);
  CHECK(base && *base && (*base)->Structures.size() == 2,
        "two separated buildings in the same cell prepare as one immutable source");
  if (!base || !*base) { return; }
  const auto indexBytes = PageIndexAsset::EncodedBytes((*base)->Structures.size());
  CHECK(indexBytes, "the grouped source index reservation is derived from its source count");
  if (!indexBytes) { return; }
  const auto index = (*assets)->ResolveSourceIndex(key, *indexBytes);
  CHECK(index && *index, "source-group selection has a persisted metadata hierarchy");
  if (!index || !*index) { return; }
  std::vector<std::vector<uint32_t>> groups;
  CHECK((*index)->Hierarchy.SelectNodesNearFirst(
            raw.AnchorEcef + (*base)->Surfaces[0].Bounds().Middle(),
            [](const auto &node) { return node.Count == 1; },
            [&](const auto &node) {
              const auto members = (*index)->Hierarchy.Members(node);
              groups.emplace_back(members.begin(), members.end());
            }) &&
            groups.size() == 2,
        "the stored hierarchy selects both complete source groups before form IO");
  if (groups.size() != 2) { return; }
  auto database = AssetCache::Open((root / "views.sqlite").string());
  CHECK(database, "the group field store opens");
  if (!database) { return; }
  const std::shared_ptr<AssetCache> cache(std::move(*database));
  auto wholeRecipe = Recipe(key);
  auto groupRecipe = wholeRecipe;
  groupRecipe.Cells.clear();
  groupRecipe.Sources = groups[0];
  const auto group = std::make_shared<BuildingImpostorSource>(groupRecipe, *assets);
  auto secondRecipe = groupRecipe;
  secondRecipe.Sources = groups[1];
  const auto neighbor = std::make_shared<BuildingImpostorSource>(secondRecipe, *assets);
  const auto before = (*assets)->Costs();
  auto metadata = Metadata(cache, group);
  auto other = Metadata(cache, neighbor);
  CHECK(metadata && other && (*assets)->Costs().SurfaceReadBytes == before.SurfaceReadBytes,
        "both subgroup bounds resolve without opening their shared form block");
  if (!metadata || !other) { return; }
  CHECK(group->Provenance() != neighbor->Provenance() &&
            Length(metadata->Metadata().Centre - other->Metadata().Centre) > 100 &&
            metadata->Metadata().HalfExtentM < other->Metadata().HalfExtentM,
        "capture centers and extents describe their own source group rather than the cell");
  const Generators::BuildingSourceBudget one{
      .Samples = 1024, .Sources = 1, .Faces = 1000, .TriangleAttempts = 100000};
  BuildingImpostorSource whole(std::move(wholeRecipe), *assets);
  CHECK(!whole.Generate({0}, kBytes, one),
        "the two-source whole cell cannot pass a one-source generation budget");
  CHECK(group->Generate({0}, kBytes, one) && neighbor->Generate({0}, kBytes, one),
        "each subgroup generates its field within the one-source budget");
  const auto cold = Resolve(cache, *metadata, group, 1);
  const auto offline = std::make_shared<BuildingImpostorSource>(groupRecipe, nullptr);
  const auto warm = Metadata(cache, offline);
  CHECK(warm, "subgroup metadata restores without a building source store");
  if (warm) {
    const auto field = Resolve(cache, *warm, offline, 0);
    CHECK(cold && field && *cold == *field,
          "the source-free group cache returns its own complete coupled material field");
  }
}

void CheckSourceIndex(Generators::PreparedBuildingAssets &assets,
                      const std::string &key,
                      size_t sources) {
  const auto bytes = PageIndexAsset::EncodedBytes(sources);
  CHECK(bytes, "the real source hierarchy reservation follows its source count");
  if (!bytes) { return; }
  const auto before = assets.Costs();
  const auto began = std::chrono::steady_clock::now();
  const auto cold = assets.ResolveSourceIndex(key, *bytes);
  const auto storedAt = std::chrono::steady_clock::now();
  const auto stored = assets.Costs();
  const auto warm = assets.ResolveSourceIndex(key, *bytes);
  const auto finishedAt = std::chrono::steady_clock::now();
  CHECK(cold && *cold && (*cold)->From == PageIndexAsset::Origin::Created &&
            (*cold)->Hierarchy.SourceOrder().size() == sources && (*cold)->ByteCount == *bytes,
        "the real hierarchy preserves every supported and unresolved source ordinal");
  CHECK(warm && *warm && (*warm)->From == PageIndexAsset::Origin::Stored &&
            (*warm)->Hierarchy.SourceOrder().size() == sources,
        "the real source tree restores directly from its own cached metadata");
  CHECK(assets.Costs().ReadBytes == stored.ReadBytes &&
            assets.Costs().SurfaceReadBytes == before.SurfaceReadBytes &&
            assets.Costs().GeometryReadBytes == before.GeometryReadBytes,
        "the real hierarchy hit opens no prepared header, form block or geometry");
  std::println("SOURCE_INDEX sources={} bytes={} build_ms={:.3f} hit_ms={:.3f} header_bytes={} "
               "hit_header_bytes={}",
               sources,
               *bytes,
               std::chrono::duration<double, std::milli>(storedAt - began).count(),
               std::chrono::duration<double, std::milli>(finishedAt - storedAt).count(),
               stored.ReadBytes - before.ReadBytes,
               assets.Costs().ReadBytes - stored.ReadBytes);
}

void Partial(const std::filesystem::path &root, std::string_view index) {
  const auto bytes = ReadTextFile(index, size_t{64} * 1024 * 1024);
  CHECK(bytes.has_value(), "the explicitly requested original index is readable");
  if (!bytes) { return; }
  Data::RawSourceCache store({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(store);
  auto assets = Generators::PreparedBuildingAssets::Open(
      outshine::Test::OpenAssetStore((root / "buildings").string()), sources);
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
  CheckSourceIndex(**assets, key, base->Structures.size());
  BuildingImpostorSource::Recipe recipe{
      .BaseKey = key,
      .Cells = {},
      .Sources = {},
      .Frame = {.Axes = {.East = {{1, 0, 0}}, .North = {{0, 1, 0}}, .Up = {{0, 0, 1}}},
                .Origin = {}},
      .Directions = {{{0, 0, 1}}},
      .WallRoofMetreMaterials = {},
      .Pixels = 32};
  for (uint32_t cell = 1; cell <= 64; ++cell) { recipe.Cells.push_back(cell); }
  auto groupRecipe = recipe;
  groupRecipe.Cells.clear();
  const auto supported = std::ranges::find_if(base->Surfaces, [](const auto &surface) {
    return surface.FaceCount() != 0 && !surface.Bounds().Empty();
  });
  CHECK(supported != base->Surfaces.end(), "the real source has a supported subgroup");
  if (supported == base->Surfaces.end()) { return; }
  groupRecipe.Sources = {static_cast<uint32_t>(supported - base->Surfaces.begin())};
  BuildingImpostorSource source(std::move(recipe), *assets);
  const auto metadata = source.ResolveMetadata(**cache);
  CHECK(!metadata && metadata.error().contains("unsupported building source ordinal"),
        "real unresolved originals prevent a complete-source metadata declaration");
  CHECK((*assets)->Costs().SurfaceReadBytes == 0 && (*assets)->Costs().SurfaceWrites == 0,
        "incomplete source rejection requires no form loading or generation");
  const auto absent = outshine::ImpostorViewAssets::Open(**cache, source.Provenance());
  CHECK(absent && !*absent, "an incomplete original set publishes no replacement metadata");
  BuildingImpostorSource group(std::move(groupRecipe), *assets);
  const auto grouped = group.ResolveMetadata(**cache);
  CHECK(grouped && group.Provenance() != source.Provenance(),
        "a complete real subgroup publishes its own metadata despite unresolved neighbors");
  CHECK((*assets)->Costs().SurfaceReadBytes == 0 && (*assets)->Costs().SurfaceWrites == 0,
        "real subgroup metadata requires neither form reads nor source regeneration");
}

}

int main(int argc, char *const *argv) {
  if (argc == 5 && std::string_view(argv[1]) == "--offline") {
    return Offline(argv[2], argv[3], argv[4]);
  }
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
    Run(root, std::filesystem::absolute(argv[0]).string());
    Groups(root / "groups");
  }
  std::filesystem::remove_all(root);
  SDL_Quit();
  return Report();
}
