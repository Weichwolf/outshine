#include "BuildingAssetFixture.h"
#include "AssetCache.h"
#include "AssetPipeline.h"
#include "BuildingImpostorSource.h"
#include "BuildingMaterials.h"
#include "Check.h"
#include "ImpostorMetadataRequest.h"
#include "PreparedStructureCodec.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <system_error>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::BuildingAssetFixture;
constexpr auto kDeadline = std::chrono::seconds(5);

struct Directory {
  std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-impostor-metadata-busy-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  ~Directory() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
    CHECK(!error, "private data is removed after every pipeline, worker and database owner ends");
  }
};

struct Fixture {
  Directory Files;
  Data::RawSourceCache Raw{{.Directory = {}, .Using = Data::RawSourceCache::Use::Off}};
  Data::SourceSet Sources{Raw};
  std::shared_ptr<Generators::PreparedBuildingAssets> Assets;
  std::shared_ptr<AssetCache> Cache;
  std::shared_ptr<BuildingImpostorSource> Source;
  std::shared_ptr<BuildingImpostorSource::MetadataPreparation> Preparation =
      std::make_shared<BuildingImpostorSource::MetadataPreparation>();
  std::unique_ptr<AssetCache> NativeRecords;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> NativeWriter{nullptr, sqlite3_close};
  Tasks Worker{1};
  AssetPipeline Pipeline{Worker,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = size_t{1024} * 1024,
                          .ReceiptBytesMost = 4096}};
  std::optional<AssetPipeline::Consumer> Consumer;
  std::optional<ImpostorMetadataRequest> Request;
  std::string Error;
  std::string Key;

  bool Open() {
    auto assets =
        Generators::PreparedBuildingAssets::Open((Files.Path / "buildings").string(), Sources);
    CHECK(assets, "the real native building service opens");
    if (!assets) { return false; }
    Assets = std::move(*assets);
    const auto input = Inputs();
    const auto heights = Heights();
    Key = Assets->Key({.Zoom = 0, .X = 0, .Y = 0}, 17, {}, 1000, "metadata-busy");
    const std::atomic_bool stopping{false};
    const auto built = Assets->Generate(Key, input, *heights, stopping);
    CHECK(built && *built, "complete native index and form products exist before contention");
    if (!built || !*built) { return false; }
    Source = std::make_shared<BuildingImpostorSource>(
        BuildingImpostorSource::Recipe{
            .BaseKey = Key,
            .Cells = {input.Structures.front().Cell.Index},
            .Frame = {.Axes = EnuAxesEcef({.LongitudeDeg = 9, .LatitudeDeg = 47}), .Origin = {}},
            .Directions = {{{0, 0, -1}}},
            .WallRoofMetreMaterials = Generators::BuildingSurfaceMaterials(),
            .Pixels = 32},
        Assets);
    const auto path = Files.Path / "views.sqlite";
    auto cache = AssetCache::Open(path.string());
    CHECK(cache, "the actual metadata store opens");
    if (!cache) { return false; }
    Cache = std::move(*cache);
    sqlite3 *writer = nullptr;
    const int status = sqlite3_open(path.c_str(), &writer);
    Writer.reset(writer);
    CHECK(status == SQLITE_OK, "a separate SQLite writer opens the metadata store");
    return status == SQLITE_OK;
  }

  bool Legacy() {
    const auto path = Files.Path / "buildings" / "assets.sqlite";
    auto opened = AssetCache::Open(path.string());
    CHECK(opened, "the original source store opens independently for legacy replacement");
    if (!opened) { return false; }
    NativeRecords = std::move(*opened);
    const auto found = NativeRecords->Find(Key);
    const auto heights = Heights();
    const auto original = Generators::PrepareStructureTile(Inputs(), *heights);
    CHECK(found && *found && original, "the same enriched native source retains its logical key");
    if (!found || !*found || !original) { return false; }
    auto record = **found;
    const auto bytes = Generators::EncodePreparedStructureTile(*original);
    CHECK(bytes, "the supported complete native source encodes");
    if (!bytes) { return false; }
    record.Package.clear();
    record.ByteCount = bytes->size();
    CHECK(NativeRecords->Publish(std::span(&record, 1), *bytes),
          "the source key now names the supported complete legacy product");
    sqlite3 *writer = nullptr;
    const auto status = sqlite3_open(path.c_str(), &writer);
    NativeWriter.reset(writer);
    CHECK(status == SQLITE_OK, "a real native-source writer controls migration admission");
    return status == SQLITE_OK;
  }

  bool Start() {
    auto consumer = Pipeline.OpenConsumer({.Identity = Cache});
    CHECK(consumer, "the actual source metadata consumer is admitted");
    if (!consumer) { return false; }
    Consumer = std::move(*consumer);
    auto request = ImpostorMetadataRequest::Start(
        Pipeline,
        *Consumer,
        Worker,
        Cache,
        Source->Provenance(),
        [cache = Cache, source = Source, preparation = Preparation](const std::stop_token &stop) {
          return source->ResolveMetadata(*cache, *preparation, stop);
        },
        size_t{128} * 1024);
    CHECK(request, "the existing bounded metadata pipeline owns the request");
    if (!request) { return false; }
    Request = std::move(*request);
    return true;
  }

  bool Ready() {
    if (!Request) {
      Error = "metadata request unavailable";
      return false;
    }
    const auto ready = Request->Poll();
    if (!ready) {
      Error = ready.error();
      return false;
    }
    return *ready && (**ready).Metadata().WorldFromField.has_value();
  }

  void Cancel() {
    CHECK(Request.has_value(), "a live request owns the pending metadata preparation");
    if (Request) { Request->Cancel(); }
  }
};

void ResumeAfterWriter() {
  Fixture fixture;
  if (!fixture.Open()) { return; }
  const auto before = fixture.Assets->Costs();
  CHECK(sqlite3_exec(fixture.Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "another real connection holds metadata publication admission");
  if (!fixture.Start()) { return; }
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
  bool premature = false;
  while (std::chrono::steady_clock::now() < until && fixture.Error.empty()) {
    premature = fixture.Ready() || premature;
    (void)fixture.Worker.AwaitCompletion(0.001);
  }
  const auto held = fixture.Assets->Costs();
  CHECK(!premature && held.ReadBytes > before.ReadBytes &&
            held.SurfaceReadBytes == before.SurfaceReadBytes,
        "metadata reaches its source index but needs no form IO or partial publication");
  const auto independent = fixture.Worker.Post([] {});
  CHECK(independent != Tasks::kNoTask && fixture.Worker.AwaitCompletion(independent, 1),
        "the held writer leaves the single worker available for unrelated work");
  if (independent != Tasks::kNoTask) { (void)fixture.Worker.TakeCompletion(independent); }
  CHECK(fixture.Preparation->Preparations() == 1 && fixture.Preparation->PublicationBytes() > 0 &&
            fixture.Pipeline.Counts().Jobs == 1,
        "the existing admitted job retains exactly one prepared metadata package");
  CHECK(sqlite3_exec(fixture.Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the independent metadata writer releases admission");
  bool complete = false;
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  do {
    complete = fixture.Ready();
    if (complete || !fixture.Error.empty()) { break; }
    (void)fixture.Worker.AwaitCompletion(0.001);
  } while (std::chrono::steady_clock::now() < deadline);
  CHECK(complete,
        fixture.Error.empty() ? "the same admitted metadata request resumes after writer release"
                              : fixture.Error.c_str());
  CHECK(fixture.Assets->Costs().ReadBytes == held.ReadBytes &&
            fixture.Assets->Costs().SurfaceReadBytes == held.SurfaceReadBytes,
        "publication retries repeat neither native source decode nor fine form reads");
  CHECK(fixture.Preparation->Preparations() == 1 && fixture.Preparation->PublicationBytes() == 0,
        "successful publication releases its retained package without another preparation");
}

void CancelPendingPublication() {
  Fixture fixture;
  if (!fixture.Open()) { return; }
  CHECK(sqlite3_exec(fixture.Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "another writer holds the cancellable metadata publication");
  if (!fixture.Start()) { return; }
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
  while (std::chrono::steady_clock::now() < until && fixture.Error.empty()) {
    CHECK(!fixture.Ready(), "the held metadata product cannot be exposed prematurely");
    (void)fixture.Worker.AwaitCompletion(0.001);
  }
  const auto barrier = fixture.Worker.Post([] {});
  CHECK(barrier != Tasks::kNoTask && fixture.Worker.AwaitCompletion(barrier, 1),
        "the cancellation probe waits for the current atomic worker step");
  if (barrier != Tasks::kNoTask) { (void)fixture.Worker.TakeCompletion(barrier); }
  CHECK(fixture.Error.empty() && fixture.Preparation->Preparations() == 1 &&
            fixture.Preparation->PublicationBytes() > 0,
        "cancellation reaches a genuine retained prepared package");
  const std::weak_ptr<BuildingImpostorSource::MetadataPreparation> pending = fixture.Preparation;
  fixture.Preparation.reset();
  fixture.Cancel();
  const auto stopped = fixture.Worker.Post([] {});
  CHECK(stopped != Tasks::kNoTask && fixture.Worker.AwaitCompletion(stopped, 1),
        "cancellation does not block the content worker");
  if (stopped != Tasks::kNoTask) { (void)fixture.Worker.TakeCompletion(stopped); }
  CHECK(pending.expired() && fixture.Pipeline.Counts().InputBytes == 0,
        "the cancelled job releases its own preparation and input reservation");
  CHECK(sqlite3_exec(fixture.Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the writer releases admission after the job is cancelled");
  const auto absent =
      Content::ImpostorViewAssets::Open(*fixture.Cache, fixture.Source->Provenance());
  CHECK(absent && !*absent, "cancelled work never starts publication after writer release");
}

void NativeMigrationRetainsInput() {
  Fixture fixture;
  if (!fixture.Open() || !fixture.Legacy()) { return; }
  const auto before = fixture.Assets->Costs();
  CHECK(sqlite3_exec(fixture.NativeWriter.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "the native source writer holds the first migration publication");
  if (!fixture.Start()) { return; }
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
  while (std::chrono::steady_clock::now() < until && fixture.Error.empty()) {
    CHECK(!fixture.Ready(), "metadata cannot consume an unfinished native migration");
    (void)fixture.Worker.AwaitCompletion(0.001);
  }
  const auto barrier = fixture.Worker.Post([] {});
  CHECK(barrier != Tasks::kNoTask && fixture.Worker.AwaitCompletion(barrier, 1),
        "the native continuation releases the single worker between steps");
  if (barrier != Tasks::kNoTask) { (void)fixture.Worker.TakeCompletion(barrier); }
  const auto waiting = fixture.Assets->Costs();
  CHECK(fixture.Error.empty() && waiting.LegacyPreparations == before.LegacyPreparations + 1 &&
            waiting.BasePackagePreparations == before.BasePackagePreparations + 1 &&
            fixture.Preparation->PublicationBytes() > 0 && fixture.Preparation->Preparations() == 0,
        "one admitted metadata job retains its decoded source and first migration package");
  CHECK(sqlite3_exec(fixture.NativeWriter.get(), "ROLLBACK", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "the native source writer releases migration admission");
  bool complete = false;
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  do {
    complete = fixture.Ready();
    if (complete || !fixture.Error.empty()) { break; }
    (void)fixture.Worker.AwaitCompletion(0.001);
  } while (std::chrono::steady_clock::now() < deadline);
  CHECK(complete,
        fixture.Error.empty() ? "the same metadata job completes its source migration"
                              : fixture.Error.c_str());
  const auto after = fixture.Assets->Costs();
  const auto stored = fixture.NativeRecords->Load(fixture.Key, size_t{128} * 1024 * 1024);
  CHECK(
      stored && *stored &&
          Generators::DecodePreparedStructureIndex((**stored).Bytes(), size_t{128} * 1024 * 1024) &&
          after.LegacyPreparations == before.LegacyPreparations + 1 &&
          after.BasePackagePreparations == before.BasePackagePreparations + 3 &&
          fixture.Preparation->Preparations() == 1,
      "one cell/index/basis conversion finishes under the original key before metadata commits");
}
}

int main() {
  ResumeAfterWriter();
  CancelPendingPublication();
  NativeMigrationRetainsInput();
  return Report();
}
