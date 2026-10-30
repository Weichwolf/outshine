#include "TerrainFieldPreparation.h"
#include "Check.h"
#include "PreparedTerrainAssets.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "TilePool.h"
#include "Tasks.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <semaphore>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace std::chrono_literals;
constexpr Data::TileId kTile{.Zoom = 12, .X = 1200, .Y = 1500};

class NoTransport final : public Data::SourceTransport {
public:
  Data::FetchStart Begin(const std::string & /*url*/) override { return Data::Ticket::None; }

  Data::Wire Collect(Data::Ticket /*ticket*/) override { return Data::Wire::Never(); }

  void Cancel(Data::Ticket /*ticket*/) override {}
};

struct Fixture {
  Fixture()
      : Root_(std::filesystem::temp_directory_path() /
              ("outshine-field-writer-" +
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))),
        Raw_({.Directory = {}, .Using = Data::RawSourceCache::Use::Off}),
        Sources(Raw_) {
    auto opened = Generators::PreparedTerrainAssets::Open(Root_.string(), Sources);
    CHECK(opened.has_value(), "the native field service opens a private cold cache");
    if (!opened) { return; }
    Assets = std::move(*opened);
    sqlite3 *writer = nullptr;
    const auto status = sqlite3_open((Root_ / "assets.sqlite").c_str(), &writer);
    Writer_.reset(writer);
    CHECK(status == SQLITE_OK, "a real second SQLite connection owns writer admission");
    if (status != SQLITE_OK) { Assets.reset(); }
  }

  ~Fixture() {
    Writer_.reset();
    Assets.reset();
    std::error_code ignored;
    std::filesystem::remove_all(Root_, ignored);
  }

  [[nodiscard]] bool Begin() const {
    const bool ready =
        Assets &&
        sqlite3_exec(Writer_.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK;
    CHECK(ready, "an independent writer prevents native publication");
    return ready;
  }

  [[nodiscard]] bool Commit() const {
    const bool ready =
        sqlite3_exec(Writer_.get(), "COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK;
    CHECK(ready, "the independent writer releases admission");
    return ready;
  }

  std::filesystem::path Root_;
  Data::RawSourceCache Raw_;

  Data::SourceSet Sources;
  std::shared_ptr<Generators::PreparedTerrainAssets> Assets;

  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer_{nullptr, sqlite3_close};
};

Ground::TerrainGrid Field(Data::TileId at) {
  Ground::TerrainField field(2, 2);
  for (uint32_t row = 0; row < 2; ++row) {
    for (uint32_t col = 0; col < 2; ++col) { field.SetM(row, col, 100); }
  }
  field.AddSource({.Tile = at, .SourceId = "field-writer", .Revision = "1"});
  return Ground::TerrainGrid::Holding(std::move(field));
}

void OwnedField() {
  const Fixture fixture;
  if (!fixture.Begin()) { return; }
  const auto &assets = fixture.Assets;
  const auto request = assets->RequestField(kTile, {});
  const auto emptyBytes = request->HeapBytes();
  unsigned factories = 0;
  const auto first = request->Resume([&] {
    ++factories;
    return Field(kTile);
  });
  CHECK(first.Grid.Where() == Ground::TerrainGrid::State::Deferred && first.CacheDeferred,
        "native writer contention is Deferred, never a permanent source refusal");
  CHECK(factories == 1 && assets->Costs().FieldPreparations == 1 && assets->Costs().Writes == 0,
        "one generated complete field retains one prepared unpublished package");
  const auto pendingBytes = request->HeapBytes();
  CHECK(pendingBytes > emptyBytes && request->Memory()->load() == pendingBytes,
        "retained package capacity is visible while writer admission is unavailable");
  const auto same = assets->RequestField(kTile, {});
  CHECK(same == request, "equal field consumers coalesce at the existing weak request entry");
  const auto repeated = same->Resume([&] {
    ++factories;
    return Ground::TerrainGrid::Refused();
  });
  CHECK(repeated.Grid.Where() == Ground::TerrainGrid::State::Deferred && factories == 1 &&
            assets->Costs().FieldPreparations == 1 && request->HeapBytes() == pendingBytes,
        "another writer attempt reuses every encoded byte without invoking a factory");
  const auto attempts = assets->Costs().FieldPublicationAttempts;
  std::stop_source stopped;
  stopped.request_stop();
  const auto canceled = request->Resume({}, stopped.get_token());
  CHECK(canceled.Grid.Where() == Ground::TerrainGrid::State::Deferred &&
            assets->Costs().FieldPublicationAttempts == attempts,
        "a canceled caller admits no additional publication attempt");
  if (!fixture.Commit()) { return; }
  const auto ready = request->Resume([&] {
    ++factories;
    return Ground::TerrainGrid::Refused();
  });
  CHECK(ready.Grid.TryField() && ready.Grid.TryField()->AtM(0, 0) == 100 && factories == 1 &&
            assets->Costs().FieldPreparations == 1 && assets->Costs().Writes == 1,
        "resumption with a fresh borrowed factory publishes and restores the original field");
  CHECK(request->HeapBytes() == emptyBytes,
        "the published package releases its capacity before immutable readers consume the field");
  const auto resident = same->Resume({});
  CHECK(resident.Grid.TryField() == ready.Grid.TryField(),
        "coalesced consumers retain exactly the same completed native field");
}

void SourceWaitAndCancellationStayDistinct() {
  const Fixture fixture;
  if (!fixture.Begin()) { return; }
  const auto request = fixture.Assets->RequestField(kTile, {});
  const auto waiting = request->Resume([] { return Ground::TerrainGrid::Deferred(); });
  CHECK(waiting.Grid.Where() == Ground::TerrainGrid::State::Deferred && !waiting.CacheDeferred &&
            fixture.Assets->Costs().FieldPreparations == 0,
        "source-deferred acquisition is not mislabeled as a native writer retry");
  std::stop_source canceled;
  const auto stopped = request->Resume(
      [&] {
        canceled.request_stop();
        return Field(kTile);
      },
      canceled.get_token());
  CHECK(stopped.Grid.Where() == Ground::TerrainGrid::State::Deferred &&
            fixture.Assets->Costs().FieldPreparations == 0 &&
            fixture.Assets->Costs().FieldPublicationAttempts == 0,
        "cancellation inside a borrowed factory prevents encoding or an unpublished Ready result");
  const auto generated = request->Resume([] { return Field(kTile); });
  CHECK(generated.Grid.Where() == Ground::TerrainGrid::State::Deferred && generated.CacheDeferred &&
            fixture.Assets->Costs().FieldPreparations == 1,
        "a later valid caller can advance the same native request from source waiting to writer "
        "waiting");
  if (!fixture.Commit()) { return; }
  CHECK(request->Resume({}).Grid.TryField() && fixture.Assets->Costs().Writes == 1,
        "the completed native package resumes independently of its former source callback");
  const auto invalid = fixture.Assets->RequestField({.Zoom = -1}, {})->Resume({});
  CHECK(invalid.Grid.Where() == Ground::TerrainGrid::State::Refused && invalid.Grid.Failure() &&
            invalid.Grid.Failure()->Reason == Data::SourceFailureReason::InvalidRequest,
        "terminal invalid input is never retried as writer contention");
}

void ConcurrentCallsKeepTheirOwnWaitReason() {
  const Fixture fixture;
  if (!fixture.Assets) { return; }
  const auto request = fixture.Assets->RequestField(kTile, {});
  std::binary_semaphore entered(0);
  std::binary_semaphore release(0);
  std::optional<Ground::TerrainTiles::FieldStep> source;
  std::thread owner([&] {
    source = request->Resume([&] {
      entered.release();
      release.acquire();
      return Ground::TerrainGrid::Deferred();
    });
  });
  CHECK(entered.try_acquire_for(5s),
        "one coalesced native caller lends an unfinished source factory");
  const auto concurrent = request->Resume([] { return Field(kTile); });
  CHECK(concurrent.Grid.Where() == Ground::TerrainGrid::State::Deferred && concurrent.CacheDeferred,
        "another caller yields immediately while native request state is owned elsewhere");
  release.release();
  owner.join();
  CHECK(source && source->Grid.Where() == Ground::TerrainGrid::State::Deferred &&
            !source->CacheDeferred && fixture.Assets->Costs().FieldPreparations == 0,
        "a coalesced native wait cannot change the original caller's source-dependency state");
}

Ground::TilePool::Config Config(const Fixture &fixture, Tasks &compute) {
  Ground::TilePool::Config config;
  config.Compute = &compute;
  config.Carriers = 1;
  config.OutstandingMost = 1;
  config.PreparedFields = [assets = fixture.Assets](Data::TileId at,
                                                    const Ground::TerrainTiles::Shaped &shape) {
    return assets->RequestField(at, shape);
  };
  return config;
}

bool Attempted(Ground::TilePool &pool, const Fixture &fixture, uint64_t previous) {
  const auto until = std::chrono::steady_clock::now() + 5s;
  Ground::TilePool::LandingCursor cursor;
  while (fixture.Assets->Costs().FieldPublicationAttempts == previous &&
         std::chrono::steady_clock::now() < until) {
    static_cast<void>(pool.AwaitLanding(0.005, cursor));
  }
  return fixture.Assets->Costs().FieldPublicationAttempts > previous;
}

void SchedulerOwnsPendingField() {
  Fixture fixture;
  if (!fixture.Begin()) { return; }
  Tasks compute(1);
  NoTransport wire;
  Ground::TilePool pool(Config(fixture, compute), fixture.Sources, wire);
  pool.Shapes({.Kind = "plane"});
  const auto beforeBytes = pool.SchedulerBytes();
  std::shared_ptr<const Ground::TerrainField> field;
  CHECK(pool.Field(kTile, &field) == Ground::TilePool::Reply::Pending,
        "the first native field is admitted as one background reservation");
  CHECK(Attempted(pool, fixture, 0), "the native field reaches a real blocked publication");
  const auto sentinel = compute.Post([] {});
  const auto until = std::chrono::steady_clock::now() + 5s;
  bool completed = false;
  while (!completed && std::chrono::steady_clock::now() < until) {
    completed = compute.TakeCompletion(sentinel);
    if (!completed) { static_cast<void>(compute.AwaitCompletion(0.005)); }
  }
  CHECK(completed, "writer-deferred terrain gives the only worker to unrelated compute");
  CHECK(pool.SchedulerBytes() > beforeBytes && pool.Counters().Outstanding == 1 &&
            pool.Counters().Posts == 1 && pool.Counters().ParkedJobs == 0,
        "the native package stays counted against its original admission outside source parking");
  CHECK(pool.Field({.Zoom = kTile.Zoom, .X = kTile.X + 1, .Y = kTile.Y}, &field) ==
            Ground::TilePool::Reply::Deferred,
        "a retained native field cannot silently admit another product past capacity one");
  Ground::TilePool::LandingCursor cursor;
  CHECK(!pool.AwaitLanding(0.025, cursor), "writer retries do not report a completed field");
  CHECK(fixture.Assets->Costs().FieldPreparations == 1 &&
            fixture.Assets->Costs().FieldPublicationAttempts < 64,
        "failing writer attempts preserve preparation and yield between bounded retry deadlines");
  if (!fixture.Commit()) { return; }
  CHECK(pool.AwaitLanding(5.0, cursor),
        "the waiting caller dispatches the due native continuation");
  CHECK(pool.Field(kTile, &field) == Ground::TilePool::Reply::Ready && field &&
            fixture.Assets->Costs().FieldPreparations == 1 && fixture.Assets->Costs().Writes == 1 &&
            pool.Counters().Posts == 1,
        "the original field completes without another reservation, stitch or encode");
}

void OneWorkerRetainsIndependentNativeRequests() {
  Fixture fixture;
  if (!fixture.Begin()) { return; }
  Tasks compute(1);
  NoTransport wire;
  auto config = Config(fixture, compute);
  config.OutstandingMost = 2;
  Ground::TilePool pool(config, fixture.Sources, wire);
  pool.Shapes({.Kind = "sineGrid", .AmplitudeM = 30, .WavelengthM = 100});
  constexpr Data::TileId other{.Zoom = kTile.Zoom, .X = kTile.X + 4, .Y = kTile.Y};
  std::shared_ptr<const Ground::TerrainField> first;
  std::shared_ptr<const Ground::TerrainField> second;
  CHECK(pool.Field(kTile, &first) == Ground::TilePool::Reply::Pending &&
            Attempted(pool, fixture, 0),
        "the sole worker prepares and pauses its first native field");
  const auto attempts = fixture.Assets->Costs().FieldPublicationAttempts;
  CHECK(pool.Field(other, &second) == Ground::TilePool::Reply::Pending &&
            Attempted(pool, fixture, attempts),
        "the same worker can prepare another tile while its first encoded field remains pending");
  const auto until = std::chrono::steady_clock::now() + 5s;
  while (fixture.Assets->Costs().FieldPreparations < 2 &&
         std::chrono::steady_clock::now() < until) {
    static_cast<void>(pool.AwaitLanding(0.005));
  }
  CHECK(fixture.Assets->Costs().FieldPreparations == 2 && pool.Counters().Outstanding == 2,
        "both independent packages belong to their own original reservations");
  if (!fixture.Commit()) { return; }
  const auto finish = std::chrono::steady_clock::now() + 5s;
  while ((!first || !second) && std::chrono::steady_clock::now() < finish) {
    static_cast<void>(pool.AwaitLanding(0.005));
    if (!first) { static_cast<void>(pool.Field(kTile, &first)); }
    if (!second) { static_cast<void>(pool.Field(other, &second)); }
  }
  CHECK(first && second && fixture.Assets->Costs().FieldPreparations == 2 &&
            fixture.Assets->Costs().Writes == 2 && pool.Counters().Posts == 2,
        "both native products resume without stitching or encoding either field again");
  CHECK(first && second && !first->Sources().empty() && !second->Sources().empty() &&
            std::ranges::any_of(first->Sources(),
                                [](const auto &source) { return source.Tile == kTile; }) &&
            std::ranges::any_of(second->Sources(),
                                [other](const auto &source) { return source.Tile == other; }) &&
            std::ranges::none_of(first->Sources(),
                                 [other](const auto &source) { return source.Tile == other; }) &&
            std::ranges::none_of(second->Sources(),
                                 [](const auto &source) { return source.Tile == kTile; }),
        "worker reuse cannot exchange the retained products or their source addresses");
}

void CoalescedFieldAndMeshCountNativeStorageOnce() {
  Fixture fixture;
  if (!fixture.Begin()) { return; }
  Tasks compute(1);
  NoTransport wire;
  auto config = Config(fixture, compute);
  config.OutstandingMost = 2;
  Ground::TilePool pool(config, fixture.Sources, wire);
  const Ground::TerrainTiles::Shaped shape{
      .Kind = "sineGrid", .AmplitudeM = 30, .WavelengthM = 100};
  pool.Shapes(
      {.Kind = shape.Kind, .AmplitudeM = shape.AmplitudeM, .WavelengthM = shape.WavelengthM});
  std::shared_ptr<const Ground::TerrainField> field;
  CHECK(pool.Field(kTile, &field) == Ground::TilePool::Reply::Pending &&
            Attempted(pool, fixture, 0),
        "one native field owns a prepared package before a mesh requests the same input");
  const auto native = fixture.Assets->RequestField(kTile, shape);
  const auto bytes = native->HeapBytes();
  const auto before = pool.SchedulerBytes();
  const auto attempts = fixture.Assets->Costs().FieldPublicationAttempts;
  CHECK(pool.Wants(kTile, 4) == Ground::TilePool::Reply::Pending &&
            Attempted(pool, fixture, attempts),
        "a mesh reservation can share the paused native field on the same reused worker");
  CHECK(
      fixture.Assets->Costs().FieldPreparations == 1 && pool.SchedulerBytes() < before + bytes / 2,
      "coalesced reservations count the large encoded package once, plus their small job metadata");
  if (!fixture.Commit()) { return; }
  TileBuild mesh;
  CHECK(pool.MeshAwaited(kTile, 4, &mesh) == Ground::TilePool::Reply::Ready,
        "the coalesced mesh consumes the native field after writer admission resumes");
  const auto until = std::chrono::steady_clock::now() + 5s;
  while (!field && std::chrono::steady_clock::now() < until) {
    static_cast<void>(pool.AwaitLanding(0.005));
    static_cast<void>(pool.Field(kTile, &field));
  }
  CHECK(field && !mesh.Nodes.empty() && fixture.Assets->Costs().Writes == 1 &&
            fixture.Assets->Costs().FieldPreparations == 1,
        "field and mesh consumers finish with a single native preparation and publication");
}

void OwnerEndCancelsPausedField() {
  Fixture fixture;
  if (!fixture.Begin()) { return; }
  Tasks compute(1);
  NoTransport wire;
  std::weak_ptr<Ground::TerrainTiles::FieldRequest> retired;
  {
    auto config = Config(fixture, compute);
    config.PreparedFields =
        [&, assets = fixture.Assets](Data::TileId at, const Ground::TerrainTiles::Shaped &shape) {
          auto request = assets->RequestField(at, shape);
          retired = request;
          return request;
        };
    Ground::TilePool pool(config, fixture.Sources, wire);
    pool.Shapes({.Kind = "plane"});
    std::shared_ptr<const Ground::TerrainField> field;
    CHECK(pool.Field(kTile, &field) == Ground::TilePool::Reply::Pending &&
              Attempted(pool, fixture, 0),
          "the ending pool owns a paused unpublished field");
  }
  CHECK(retired.expired() && fixture.Assets->Costs().Writes == 0,
        "owner end releases the paused request and its native package without a late publication");
  if (!fixture.Commit()) { return; }
  unsigned factories = 0;
  const auto missing = fixture.Assets->RequestField(kTile, {.Kind = "plane"})->Resume([&] {
    ++factories;
    return Ground::TerrainGrid::Refused();
  });
  CHECK(missing.Grid.Where() == Ground::TerrainGrid::State::Refused && factories == 1,
        "releasing the external writer cannot revive the dead pool's unpublished work");
}

void ScopeReplacesPendingAdmission() {
  Fixture fixture;
  if (!fixture.Begin()) { return; }
  Tasks compute(1);
  NoTransport wire;
  Ground::TilePool pool(Config(fixture, compute), fixture.Sources, wire);
  pool.Shapes({.Kind = "plane", .Seed = 1});
  std::shared_ptr<const Ground::TerrainField> field;
  CHECK(pool.Field(kTile, &field) == Ground::TilePool::Reply::Pending &&
            Attempted(pool, fixture, 0),
        "the old terrain scope retains an unpublished native field");
  const auto attempts = fixture.Assets->Costs().FieldPublicationAttempts;
  pool.Shapes({.Kind = "plane", .Seed = 2});
  auto replaced = pool.Field(kTile, &field);
  const auto until = std::chrono::steady_clock::now() + 5s;
  while (replaced == Ground::TilePool::Reply::Deferred &&
         std::chrono::steady_clock::now() < until) {
    static_cast<void>(pool.AwaitLanding(0.005));
    replaced = pool.Field(kTile, &field);
  }
  CHECK(replaced == Ground::TilePool::Reply::Pending && Attempted(pool, fixture, attempts),
        "the same tile receives a new scoped admission once the old queued owner fits its bound");
  CHECK(fixture.Assets->Costs().FieldPreparations == 2 && fixture.Assets->Costs().Writes == 0 &&
            pool.Counters().Posts == 2,
        "each scope prepares once and neither blocked writer commits prematurely");
  if (!fixture.Commit()) { return; }
  CHECK(pool.AwaitLanding(5.0) && pool.Field(kTile, &field) == Ground::TilePool::Reply::Ready &&
            fixture.Assets->Costs().Writes == 1,
        "a stale queued field cannot publish or remove the replacement admission");
  unsigned oldFactories = 0;
  const auto old = fixture.Assets->RequestField(kTile, {.Kind = "plane", .Seed = 1})->Resume([&] {
    ++oldFactories;
    return Ground::TerrainGrid::Refused();
  });
  CHECK(old.Grid.Where() == Ground::TerrainGrid::State::Refused && oldFactories == 1,
        "the canceled scope has no silently published cache entry");
}

void AwaitedMeshResumesAfterWriterRelease() {
  Fixture fixture;
  if (!fixture.Begin()) { return; }
  Tasks compute(1);
  NoTransport wire;
  Ground::TilePool pool(Config(fixture, compute), fixture.Sources, wire);
  pool.Shapes({.Kind = "plane"});
  std::atomic_bool committed{false};
  std::thread release([&] {
    const auto until = std::chrono::steady_clock::now() + 5s;
    while (fixture.Assets->Costs().FieldPublicationAttempts == 0 &&
           std::chrono::steady_clock::now() < until) {
      std::this_thread::yield();
    }
    committed = fixture.Commit();
  });
  TileBuild mesh;
  const auto result = pool.MeshAwaited(kTile, 4, &mesh);
  release.join();
  CHECK(committed && result == Ground::TilePool::Reply::Ready && !mesh.Nodes.empty() &&
            fixture.Assets->Costs().FieldPreparations == 1 && fixture.Assets->Costs().Writes == 1,
        "MeshAwaited wakes at a writer deadline without a source/network completion event");
}

void AnotherProducerSatisfiesSourceWait() {
  const Fixture fixture;
  if (!fixture.Assets) { return; }
  const auto request = fixture.Assets->RequestField(kTile, {});
  const auto pending = request->Resume([] { return Ground::TerrainGrid::Deferred(); });
  CHECK(pending.Grid.Where() == Ground::TerrainGrid::State::Deferred && !pending.CacheDeferred,
        "the first source wait remains distinct from cache writer contention");
  auto other = Generators::PreparedTerrainAssets::Open(fixture.Root_.string(), fixture.Sources);
  CHECK(other.has_value(), "an independent native producer opens the shared asset database");
  if (!other) { return; }
  const auto published = (*other)->RequestField(kTile, {})->Resume([] { return Field(kTile); });
  CHECK(published.Grid.TryField() && (*other)->Costs().Writes == 1,
        "the independent producer has durably published the complete native field");
  size_t unwantedFactories = 0;
  const auto resumed = request->Resume([&] {
    ++unwantedFactories;
    return Ground::TerrainGrid::Refused();
  });
  CHECK(resumed.Grid.TryField() && unwantedFactories == 0,
        "a resumed source wait observes a new native asset before retrying its unavailable source");
}
}

int main() {
  OwnedField();
  AnotherProducerSatisfiesSourceWait();
  SourceWaitAndCancellationStayDistinct();
  ConcurrentCallsKeepTheirOwnWaitReason();
  SchedulerOwnsPendingField();
  OneWorkerRetainsIndependentNativeRequests();
  CoalescedFieldAndMeshCountNativeStorageOnce();
  OwnerEndCancelsPausedField();
  ScopeReplacesPendingAdmission();
  AwaitedMeshResumesAfterWriterRelease();
  return Report();
}
