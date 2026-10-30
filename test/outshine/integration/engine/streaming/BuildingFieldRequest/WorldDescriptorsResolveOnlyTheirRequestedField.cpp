#include "BuildingFieldRequest.h"
#include "BuildingImpostorSource.h"
#include "BuildingMaterials.h"
#include "TileGeodesy.h"
#include "test/harness/engine/NativeBuildingSources.h"
#include "Check.h"
#include "Sha256.h"
#include "PreparedStructureCodec.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <span>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::BuildingSources;
constexpr size_t kMiB = size_t{1024} * 1024;

BuildingFieldRequest::Demand Demand() {
  return {.Directions = {{{0, 0, -1}}, {{1, 0, 0}}}, .Pixels = 32, .View = {0}};
}

BuildingFieldRequest::Budget Budget() {
  return {.ProducerBytes = kMiB,
          .ResidentBytes = kMiB,
          .FieldBytesMost = kMiB,
          .Capture = {.Samples = 1024, .Sources = 100, .Faces = 1000, .TriangleAttempts = 100000}};
}

std::expected<std::shared_ptr<const BuildingFieldRequest::Product>, std::string>
Await(BuildingFieldRequest &request) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  do {
    auto ready = request.Poll();
    if (!ready || *ready) { return ready; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  return std::unexpected("native World field request deadline expired");
}

std::string Hash(const ImpostorAtlas &field) {
  std::string error;
  const auto bytes = field.Encode("native-field-control", error);
  CHECK(bytes, "the native field encodes for a value comparison");
  return bytes ? Sha256Hex(bytes->data(), bytes->size()) : std::string{};
}

void Boundaries(const Fixture &fixture, const BuildingSourceDemand::Source &source) {
  auto opened = AssetCache::Open((fixture.Directory.Path / "boundaries.sqlite").string());
  CHECK(opened, "the independent boundary store opens");
  if (!opened) { return; }
  const std::shared_ptr<AssetCache> cache(std::move(*opened));
  Tasks workers(1);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 2,
                          .ConsumersMost = 2,
                          .TicketsMost = 4,
                          .InputBytesMost = 8 * kMiB,
                          .ReceiptBytesMost = 16384});
  auto invalid = Demand();
  invalid.View.Value = 2;
  CHECK(!BuildingFieldRequest::Start(pipeline, workers, cache, nullptr, source, invalid, Budget()),
        "a view outside its declared catalogue is refused before admission");
  auto limited = Budget();
  limited.Capture.Samples = 1023;
  CHECK(!BuildingFieldRequest::Start(pipeline, workers, cache, nullptr, source, Demand(), limited),
        "an insufficient capture budget is refused before work");
  const auto before = fixture.Stack.BuildingAssets()->Costs();
  AssetPipeline full(workers,
                     {.JobsMost = 1,
                      .ConsumersMost = 1,
                      .TicketsMost = 1,
                      .InputBytesMost = 1,
                      .ReceiptBytesMost = 4096});
  const auto deferred = BuildingFieldRequest::Start(
      full, workers, cache, fixture.Stack.BuildingAssets(), source, Demand(), Budget());
  CHECK(!deferred && deferred.error() == AssetPublicationError::ResourceDeferred &&
            full.Counts().Consumers == 0 && full.Counts().Jobs == 0,
        "failed admission releases its consumer and performs no partial request");
  std::atomic_bool released{false};
  const auto blocker = workers.Post([&released] { released.wait(false); });
  {
    auto request = BuildingFieldRequest::Start(
        pipeline, workers, cache, fixture.Stack.BuildingAssets(), source, Demand(), Budget());
    CHECK(request, "a held request is admitted for cancellation");
    if (request) {
      auto moved = std::move(*request);
      CHECK(!request->Poll(), "a moved-from request has no usable publication");
      moved.Cancel();
      CHECK(!moved.Poll(), "cancelled demand cannot publish a replacement");
    }
  }
  released.store(true);
  released.notify_all();
  workers.Wait(blocker);
  const auto barrier = workers.Post([] {});
  workers.Wait(barrier);
  CHECK(fixture.Stack.BuildingAssets()->Costs().ReadBytes == before.ReadBytes &&
            pipeline.Counts().Consumers == 0 && pipeline.Counts().Tickets == 0,
        "cancelled queued work reads no source and releases its admission");
  auto absent =
      BuildingFieldRequest::Start(pipeline, workers, cache, nullptr, source, Demand(), Budget());
  CHECK(absent, "a separate store may look up the same descriptor independently");
  if (absent) {
    const auto result = Await(*absent);
    CHECK(!result && result.error().contains("inputs unavailable"),
          "another cache scope cannot reuse metadata or fields from the populated store");
  }
}

void Unsupported(const Fixture &fixture, BuildingSourceDemand::Source source) {
  const auto original = fixture.Stack.BuildingAssets()->Load(source.BaseKey);
  CHECK(original && *original, "the real native source opens for a partial-source counterexample");
  if (!original || !*original || (*original)->Structures.empty()) { return; }
  auto partial = **original;
  partial.Structures.push_back(partial.Structures.front());
  partial.Surfaces.emplace_back();
  const auto bytes = Generators::EncodePreparedStructureIndex(partial);
  CHECK(bytes, "a supported and an unresolved original share one valid native index");
  if (!bytes) { return; }
  Data::RawSourceCache raw({.Directory = (fixture.Directory.Path / "partial-sources").string()});
  const Data::SourceSet sources(raw);
  auto native = Generators::PreparedBuildingAssets::Open(
      (fixture.Directory.Path / "partial").string(), sources);
  auto nativeStore = AssetCache::Open((fixture.Directory.Path / "partial/assets.sqlite").string());
  auto fieldStore = AssetCache::Open((fixture.Directory.Path / "partial-fields.sqlite").string());
  CHECK(native && nativeStore && fieldStore, "the partial-source stores open");
  if (!native || !nativeStore || !fieldStore) { return; }
  source.BaseKey = Sha256Hex(bytes->data(), bytes->size());
  const AssetRecord record{.Key = source.BaseKey,
                           .Kind = "buildings",
                           .Bounds = source.Bounds,
                           .Package = {},
                           .ByteCount = bytes->size(),
                           .Parent = {}};
  CHECK((*nativeStore)->Publish(std::span(&record, 1), *bytes),
        "the partial original index is stored without fabricated source forms");
  const std::shared_ptr<AssetCache> cache(std::move(*fieldStore));
  Tasks workers(1);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 2,
                          .ConsumersMost = 2,
                          .TicketsMost = 4,
                          .InputBytesMost = 8 * kMiB,
                          .ReceiptBytesMost = 16384});
  auto request =
      BuildingFieldRequest::Start(pipeline, workers, cache, *native, source, Demand(), Budget());
  CHECK(request, "an unresolved original is discovered by the native source job");
  if (!request) { return; }
  const auto result = Await(*request);
  CHECK(!result && result.error().contains("unsupported building source ordinal"),
        "partial native sources refuse a complete replacement and retain the caller's parent");
  CHECK((*native)->Costs().SurfaceReadBytes == 0 && (*native)->Costs().SurfaceWrites == 0,
        "unsupported source metadata requires no source form generation or loading");
}

void CheckMetadataBinding(const Fixture &fixture,
                          const BuildingFieldRequest::Product &original,
                          const std::string &provenance,
                          const Content::ImpostorViewAssets::Description &description,
                          size_t variant) {
  auto opened = AssetCache::Open(
      (fixture.Directory.Path / ("binding-" + std::to_string(variant) + ".sqlite")).string());
  CHECK(opened, "each metadata binding has an independent persisted cache");
  if (!opened) { return; }
  const std::shared_ptr<AssetCache> cache(std::move(*opened));
  const auto metadata = Content::ImpostorViewAssets::Publish(
      *cache, provenance, description, original.Metadata->Receipt().Bounds);
  CHECK(metadata, "the counterexample is valid content metadata, not corrupt bytes");
  if (!metadata) { return; }
  if (description.Pixels == Demand().Pixels &&
      description.Directions[0] == Demand().Directions[0]) {
    CHECK(metadata->PublishField({0}, *original.Field, kMiB),
          "a compatible selected field is persisted under the substituted metadata");
  }
  Tasks workers(1);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 2,
                          .ConsumersMost = 2,
                          .TicketsMost = 4,
                          .InputBytesMost = 8 * kMiB,
                          .ReceiptBytesMost = 16384});
  auto request = BuildingFieldRequest::Start(
      pipeline, workers, cache, nullptr, original.Source, Demand(), Budget());
  CHECK(request, "metadata binding is checked after asynchronous request admission");
  if (!request) { return; }
  const auto result = Await(*request);
  if (variant == 0) {
    CHECK(result && *result && Hash(*(**result).Field) == Hash(*original.Field),
          "exactly bound metadata preserves the source-free cached field");
    return;
  }
  CHECK(!result, "another capture binding cannot produce a replacement");
  if (!result) {
    const char *const expected = variant == 7 ? "inputs unavailable" : "metadata does not match";
    CHECK(result.error().contains(expected),
          "another capture binding is refused before requesting a field or opening sources");
  }
}

void MetadataBindings(const Fixture &fixture, const BuildingFieldRequest::Product &original) {
  const auto &source = original.Source;
  const auto &anchor = source.AnchorEcef;
  const auto geo = Ground::EcefToGeoWgs84({.X = anchor[0], .Y = anchor[1], .Z = anchor[2]});
  BuildingImpostorSource::Recipe recipe{
      .BaseKey = source.BaseKey,
      .Cells = {},
      .Frame = {.Axes =
                    EnuAxesEcef({.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg}),
                .Origin = {}},
      .Directions = Demand().Directions,
      .WallRoofMetreMaterials = Generators::BuildingSurfaceMaterials(),
      .Pixels = Demand().Pixels};
  for (uint32_t cell = 1; cell <= 64; ++cell) {
    if ((source.Cells & (uint64_t{1} << (cell - 1))) != 0) { recipe.Cells.push_back(cell); }
  }
  const auto provenance = recipe.Key();
  CHECK(Content::ImpostorViewAssets::MetadataKey(provenance) == original.Metadata->Receipt().Key,
        "the independent cache fixture addresses the actual requested recipe");
  const auto &metadata = original.Metadata->Metadata();
  CHECK(metadata.WorldFromField, "the original metadata has a complete frame");
  if (!metadata.WorldFromField) { return; }
  std::array<Content::ImpostorViewAssets::Description, 8> descriptions;
  descriptions.fill(metadata);
  auto rotated = *metadata.WorldFromField;
  for (size_t row = 0; row < 3; ++row) {
    rotated.At(row, 0) *= -1;
    rotated.At(row, 2) *= -1;
  }
  descriptions[1].WorldFromField = rotated;
  auto translated = *metadata.WorldFromField;
  translated.At(0, 3) += 1;
  descriptions[2].WorldFromField = translated;
  descriptions[3].Pixels *= 2;
  descriptions[4].Directions[1] = {{0, 0, 1}};
  std::ranges::reverse(descriptions[5].Directions);
  descriptions[6].Directions.pop_back();
  descriptions[7].WorldFromField.reset();
  for (size_t variant = 0; variant < descriptions.size(); ++variant) {
    CheckMetadataBinding(fixture, original, provenance, descriptions[variant], variant);
  }
}

void Exercise(const Fixture &fixture, const BuildingSourceDemand::Source &source) {
  auto opened = AssetCache::Open((fixture.Directory.Path / "fields.sqlite").string());
  CHECK(opened, "the existing asset store opens for field products");
  if (!opened) { return; }
  const std::shared_ptr<AssetCache> cache(std::move(*opened));
  Tasks workers(1);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 4,
                          .ConsumersMost = 4,
                          .TicketsMost = 8,
                          .InputBytesMost = 16 * kMiB,
                          .ReceiptBytesMost = 16384});
  std::atomic_bool released{false};
  const auto blocker = workers.Post([&released] { released.wait(false); });
  const auto before = fixture.Stack.BuildingAssets()->Costs();
  auto admitted = BuildingFieldRequest::Start(
      pipeline, workers, cache, fixture.Stack.BuildingAssets(), source, Demand(), Budget());
  CHECK(admitted, "a World descriptor enters the existing metadata and field pipeline");
  std::optional<BuildingFieldRequest> request;
  if (admitted) { request.emplace(std::move(*admitted)); }
  const auto pending =
      request ? request->Poll()
              : std::expected<std::shared_ptr<const BuildingFieldRequest::Product>, std::string>(
                    std::unexpected("request not admitted"));
  CHECK(pending && !*pending &&
            fixture.Stack.BuildingAssets()->Costs().ReadBytes == before.ReadBytes,
        "a held worker proves that Start and Poll perform no caller-side source IO");
  released.store(true);
  released.notify_all();
  workers.Wait(blocker);
  if (!request) { return; }
  auto ready = Await(*request);
  CHECK(ready && *ready,
        ready ? "the requested native field becomes ready" : ready.error().c_str());
  if (!ready || !*ready) { return; }
  MetadataBindings(fixture, **ready);
  CHECK((**ready).Source.BaseKey == source.BaseKey && (**ready).Field->Pixels() == 32 &&
            (**ready).Metadata->Metadata().WorldFromField &&
            (**ready).Metadata->Metadata().WorldFromField->Translation() == source.AnchorEcef,
        "the result retains the World identity, intrinsic frame and demanded raster");
  CHECK(std::ranges::any_of((**ready).Field->Views()[0].Texels,
                            [](const auto &pixel) { return pixel.Surface != 0; }),
        "the field contains actual native source geometry");
  const auto absent = (**ready).Metadata->LoadField({1}, kMiB);
  CHECK(absent && !*absent, "an undemanded view is not generated or stored");
  const auto digest = Hash(*(**ready).Field);
  request.reset();
  CHECK(!digest.empty() && Hash(*(**ready).Field) == digest && pipeline.Counts().Tickets > 0,
        "a returned CPU lease retains its field and publication after the request is gone");
  {
    auto other = AssetCache::Open((fixture.Directory.Path / "empty-fields.sqlite").string());
    CHECK(other, "an independent empty store opens beside the populated store");
    if (other) {
      const std::shared_ptr<AssetCache> otherCache(std::move(*other));
      auto scoped = BuildingFieldRequest::Start(
          pipeline, workers, otherCache, nullptr, source, Demand(), Budget());
      CHECK(scoped, "the same pipeline admits the same source in another cache scope");
      if (scoped) {
        const auto missing = Await(*scoped);
        CHECK(!missing && missing.error().contains("inputs unavailable"),
              "same-key jobs cannot leak products between asset stores");
      }
    }
  }
  auto reopened = AssetCache::Open((fixture.Directory.Path / "fields.sqlite").string());
  CHECK(reopened, "a fresh asset connection opens the persisted products");
  if (!reopened) { return; }
  const std::shared_ptr<AssetCache> warmCache(std::move(*reopened));
  Tasks warmWorkers(1);
  AssetPipeline warmPipeline(warmWorkers,
                             {.JobsMost = 2,
                              .ConsumersMost = 2,
                              .TicketsMost = 4,
                              .InputBytesMost = 8 * kMiB,
                              .ReceiptBytesMost = 16384});
  auto warm = BuildingFieldRequest::Start(
      warmPipeline, warmWorkers, warmCache, nullptr, source, Demand(), Budget());
  CHECK(warm, "a new pipeline accepts a source-free cached field request");
  if (!warm) { return; }
  const auto hit = Await(*warm);
  CHECK(hit && *hit && Hash(*(**hit).Field) == digest,
        "a source-free warm consumer obtains the exact captured field bytes");
  ready->reset();
  CHECK(pipeline.Counts().Tickets == 0 && pipeline.Counts().Consumers == 0,
        "the last CPU lease releases both publication tickets and its cache consumer");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native source capture");
  Fixture fixture;
  const bool prepared = fixture.Open() && fixture.Generate().has_value();
  CHECK(prepared, "actual native source products are generated");
  if (!prepared) {
    SDL_Quit();
    return Report();
  }
  BuildingSourceDemand demand;
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Ready),
        "the normal World source consumer supplies the compact descriptor");
  if (demand.Entries().size() == 1) {
    const auto &source = demand.Entries()[0].Value;
    if (source) {
      Exercise(fixture, *source);
      Boundaries(fixture, *source);
      Unsupported(fixture, *source);
    }
  }
  SDL_Quit();
  return Report();
}
