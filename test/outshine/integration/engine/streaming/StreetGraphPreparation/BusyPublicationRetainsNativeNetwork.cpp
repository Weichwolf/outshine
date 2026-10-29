#include "StreetGraphPreparation.h"
#include "PreparedStreetGraph.h"
#include "GroundMaterialCatalog.h"
#include "OsmField.h"
#include "StreetField.h"
#include "RegionSurfaceRecipes.h"
#include "SourceSet.h"
#include "RawSourceCache.h"
#include "Sha256.h"
#include "Check.h"
#include <array>
#include <algorithm>
#include <span>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Generators;
using Native = Osm::PreparedStreetGraph;
using Clock = std::chrono::steady_clock;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr size_t kBytesMost = size_t{1024} * 1024;

struct Directory {
  std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-street-publication-" + std::to_string(Clock::now().time_since_epoch().count()));

  ~Directory() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};

struct Fixture {
  Directory Root;
  Data::RawSourceCache Store{{.Directory = (Root.Path / "sources").string()}};
  Data::SourceSet Sources{Store};
  Tasks Pool{1};
  std::shared_ptr<Native> Cache;
  std::shared_ptr<Osm::OsmField> Vectors;
  std::shared_ptr<Osm::StreetField> Ways;
  std::shared_ptr<const std::array<float, 4>> Heights;
  std::string Key;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};
  std::unique_ptr<StreetGraphPreparation> Job;
  std::atomic_uint32_t Generations{0}, Samples{0}, Keys{0}, Resolutions{0};
  std::weak_ptr<const Path::Network> Generated;
  std::weak_ptr<Osm::OsmField> SourceOwner;
  std::weak_ptr<Osm::StreetField> WayOwner;
  std::weak_ptr<const std::array<float, 4>> HeightOwner;
  std::weak_ptr<int> ResolverOwner;
  std::vector<uint8_t> NativeBytes;

  ~Fixture() {
    if (Job) { Job->Cancel(); }
    const auto barrier = Pool.Post([] {});
    if (barrier != Tasks::kNoTask) { Pool.Wait(barrier); }
    Job.reset();
  }

  bool Open(bool hold) {
    std::filesystem::create_directories(Root.Path / "assets");
    GroundMaterialCatalog materials;
    RegionSurfaceRecipes recipes;
    if (!materials.Load("data/world/ground-materials.json") ||
        !recipes.Load("data/world/vegetation.json", materials)) {
      return false;
    }
    const std::array<std::string, 1> layers{"streets"};
    Vectors = std::make_shared<Osm::OsmField>(14, layers);
    const std::array<Osm::OsmField::Declared, 2> declarations{
        {{.Layer = "streets", .Key = "kind", .Value = "path", .LatLon = {47, 9, 47, 9.002}},
         {.Layer = "streets",
          .Key = "kind",
          .Value = "path",
          .LatLon = {46.999, 9.001, 47.001, 9.001}}}};
    if (!Vectors->Declare(declarations, {.LongitudeDeg = 9, .LatitudeDeg = 47})) { return false; }
    Ways = std::make_shared<Osm::StreetField>();
    if (Ways->Ingest(*Vectors, recipes) != 2) { return false; }
    Heights =
        std::make_shared<const std::array<float, 4>>(std::array<float, 4>{100, 100, 100, 100});
    auto opened = Native::Open((Root.Path / "assets").string(), Sources);
    if (!opened) { return false; }
    Cache = std::move(*opened);
    auto key = Cache->Key(*Vectors, *Ways, {}, 14, Sha256Digest(Heights->data(), sizeof(*Heights)));
    if (!key) { return false; }
    Key = std::move(*key);
    sqlite3 *connection = nullptr;
    const int status = sqlite3_open((Root.Path / "assets/assets.sqlite").c_str(), &connection);
    Writer.reset(connection);
    if (status != SQLITE_OK) { return false; }
    if (hold) {
      CHECK(sqlite3_exec(Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
            "an independent SQLite transaction holds graph publication admission");
    }
    return true;
  }

  Native::Factory Factory() {
    return [this] -> std::expected<Osm::StreetGraphBuilder::Built, std::string> {
      ++Generations;
      auto built = Osm::StreetGraphBuilder::BuildOneShot(
          *Ways, Vectors->Points(), [this](LongitudeLatitude) -> std::optional<double> {
            ++Samples;
            return Heights->front();
          });
      if (!built.Graph || built.Elevated.Refused != 0) {
        return std::unexpected("fixture graph is incomplete");
      }
      Generated = built.Graph;
      const auto bytes = built.Graph->EncodeAsset(kBytesMost);
      if (!bytes) { return std::unexpected("fixture native network cannot be encoded"); }
      NativeBytes = *bytes;
      return built;
    };
  }

  void Start(const std::shared_future<void> &gate = {}, std::promise<void> *entered = nullptr) {
    SourceOwner = Vectors;
    WayOwner = Ways;
    HeightOwner = Heights;
    auto vectors = std::exchange(Vectors, {});
    auto ways = std::exchange(Ways, {});
    auto heights = std::exchange(Heights, {});
    StreetGraphPreparation::JobFactory begin = [this, vectors, ways, heights, gate, entered] {
      ++Generations;
      if (gate.valid() && entered != nullptr) {
        entered->set_value();
        (void)gate.wait_for(std::chrono::seconds(2));
      }
      return Osm::StreetGraphBuildJob::Begin(
          *vectors, *ways, [this, heights](LongitudeLatitude) -> std::optional<double> {
            ++Samples;
            return heights->front();
          });
    };
    auto resolver = StreetGraphPreparation::CachedResolver(
        Cache, [this, vectors, ways, heights]() -> std::expected<std::string, std::string> {
          ++Keys;
          return Cache->Key(
              *vectors, *ways, {}, 14, Sha256Digest(heights->data(), sizeof(*heights)));
        });
    auto owner = std::make_shared<int>(1);
    ResolverOwner = owner;
    Job = std::make_unique<StreetGraphPreparation>(
        Pool,
        std::move(begin),
        [this, resolver = std::move(resolver), owner](const Native::Factory &factory,
                                                      const std::stop_token &stop) mutable {
          ++Resolutions;
          return resolver(factory, stop);
        });
  }

  void CancelWithOccupiedWorker() {
    std::promise<void> entered;
    std::promise<void> release;
    auto ready = entered.get_future();
    auto gate = release.get_future().share();
    const auto held = Pool.Post([&entered, gate] {
      entered.set_value();
      (void)gate.wait_for(std::chrono::seconds(2));
    });
    CHECK(held != Tasks::kNoTask &&
              ready.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
          "the existing worker is occupied before deferred cancellation");
    Job->Cancel();
    CHECK(!Job->Complete() && !ResolverOwner.expired(),
          "cancellation queues owner cleanup without joining the occupied worker or claiming "
          "completion");
    release.set_value();
    if (held != Tasks::kNoTask) { Pool.Wait(held); }
  }

  void Release() const {
    CHECK(sqlite3_exec(Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases graph publication admission");
  }
};

bool Busy(const Native::Result &value) {
  if (value) { return false; }
  const auto *error = std::get_if<AssetCacheError>(&value.error());
  return error != nullptr && *error == AssetCacheError::Busy;
}

void RetainedCacheWrite() {
  Fixture fixture;
  const bool opened = fixture.Open(true);
  CHECK(opened, "native graph cache and fixed source inputs open");
  if (!opened) { return; }
  Native::Resolution state(fixture.Key);
  const auto first = fixture.Cache->Resolve(state, fixture.Factory());
  CHECK(Busy(first) && state.PublicationBytes() > 0 && !fixture.Generated.expired(),
        "typed Busy retains both the prepared bytes and actual generated native graph");
  const auto bytes = state.PublicationBytes();
  const auto samples = fixture.Samples.load();
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    const auto next = fixture.Cache->Resolve(state, fixture.Factory());
    CHECK(Busy(next) && state.PublicationBytes() == bytes && state.Preparations() == 1 &&
              fixture.Generations == 1 && fixture.Samples == samples,
          "a later attempt preserves package capacity, topology, heights and compression work");
  }
  fixture.Release();
  const auto ready = fixture.Cache->Resolve(state, fixture.Factory());
  CHECK(ready && !ready->Hit && state.PublicationBytes() == 0 && fixture.Generated.expired(),
        "commit and validated restore release the pending owners without returning the old "
        "unvalidated graph");
  if (!ready) { return; }
  const auto actual = ready->Graph.Graph->EncodeAsset(kBytesMost);
  CHECK(actual && *actual == fixture.NativeBytes && fixture.Generations == 1,
        "the restored native network exactly preserves the generated topology and profiles");
  auto reopened = Native::Open((fixture.Root.Path / "assets").string(), fixture.Sources);
  CHECK(reopened, "a separate cache owner opens the persisted native graph");
  if (!reopened) { return; }
  CHECK(sqlite3_exec(fixture.Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "a fresh warm reader also runs under independent writer contention");
  Native::Resolution warm(fixture.Key);
  const auto hit = (*reopened)->Resolve(warm, {});
  CHECK(hit && hit->Hit && fixture.Generations == 1 && warm.Preparations() == 0 &&
            warm.PublicationAttempts() == 0,
        "a native warm hit invokes no factory, height sampler or writer");
  fixture.Release();
}

bool WaitHeld(Fixture &fixture) {
  const auto until = Clock::now() + kDeadline;
  while (Clock::now() < until) {
    const auto value = fixture.Job->Collect();
    CHECK(!value, "a busy native graph is not a completed World candidate");
    if (value) { return false; }
    if (std::string_view(fixture.Job->PhaseName()) == "native-cache-busy") { return true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(false, "the real worker reaches its pending native publication");
  return false;
}

void WorkerContinuation(bool cancel) {
  Fixture fixture;
  const bool opened = fixture.Open(true);
  CHECK(opened, "the actual shared-worker source/cache fixture opens");
  if (!opened) { return; }
  fixture.Start();
  if (!WaitHeld(fixture)) { return; }
  CHECK(!fixture.Job->Complete() && fixture.SourceOwner.expired() && fixture.WayOwner.expired() &&
            fixture.HeightOwner.expired() && fixture.Keys == 1 && fixture.Generations == 1,
        "deferred publication is nonterminal and releases its original key/generation source "
        "snapshots");
  const auto samples = fixture.Samples.load();
  const auto advances = fixture.Job->Advances();
  const auto until = Clock::now() + std::chrono::milliseconds(60);
  while (Clock::now() < until) {
    const auto value = fixture.Job->Collect();
    CHECK(!value, "caller polling keeps a held publication pending");
    if (value) { return; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const auto marker = fixture.Pool.Post([] {});
  CHECK(marker != Tasks::kNoTask, "unrelated work enters the same native compute pool");
  if (marker != Tasks::kNoTask) { fixture.Pool.Wait(marker); }
  CHECK(
      fixture.Resolutions > 1 && fixture.Resolutions < 20 && fixture.Samples == samples &&
          fixture.Job->Advances() == advances && fixture.Keys == 1 && fixture.Generations == 1,
      "bounded caller readmission neither spins a worker nor repeats key, topology or height work");
  if (cancel) { fixture.CancelWithOccupiedWorker(); }
  fixture.Release();
  const auto deadline = Clock::now() + kDeadline;
  std::optional<std::expected<StreetGraphPreparation::Completed, std::string>> result;
  while (!result && Clock::now() < deadline) {
    result = fixture.Job->Collect();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(result && fixture.Job->Complete() && fixture.ResolverOwner.expired(),
        "both release and cancellation reach a terminal result after worker-owned cleanup");
  if (!result) { return; }
  if (cancel) {
    CHECK(!*result && result->error() == "street graph candidate canceled",
          "canceling a deferred publication remains an explicit canceled candidate");
    auto cache = AssetCache::Open((fixture.Root.Path / "assets/assets.sqlite").string());
    CHECK(cache, "the canceled key can be inspected independently");
    if (!cache) { return; }
    const auto absent = (*cache)->Find(fixture.Key);
    CHECK(absent && !*absent, "cancelled native data never commits after admission returns");
  } else {
    CHECK(
        *result && result->value().Graph.Graph && !result->value().CacheHit &&
            result->value().ReadBytes > 0 && result->value().WorkerMs > 0 &&
            fixture.Generations == 1 && fixture.Samples == samples,
        "the original World worker returns exactly one validated graph without another generation");
  }
}

void InvalidNativeReplacement() {
  Fixture fixture;
  const bool opened = fixture.Open(false);
  CHECK(opened, "the stale native-format replacement fixture opens");
  if (!opened) { return; }
  auto store = AssetCache::Open((fixture.Root.Path / "assets/assets.sqlite").string());
  CHECK(store, "the existing cache row can be independently inspected");
  if (!store) { return; }
  const std::array<uint8_t, 3> bytes{1, 2, 3};
  const AssetRecord original{.Key = fixture.Key,
                             .Kind = "street-network",
                             .Bounds = {.Min = {}, .Max = {}},
                             .Package = {},
                             .ByteCount = bytes.size(),
                             .Parent = {}};
  CHECK((*store)->Publish(std::span(&original, 1), bytes),
        "a valid cache envelope contains an unsupported native graph payload");
  const auto before = (*store)->Load(fixture.Key, kBytesMost);
  CHECK(before && *before, "the original row remains readable by its native-cache envelope");
  CHECK(sqlite3_exec(fixture.Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "an independent writer holds replacement publication admission");
  Native::Resolution state(fixture.Key);
  const auto pending = fixture.Cache->Resolve(state, fixture.Factory());
  const auto held = (*store)->Load(fixture.Key, kBytesMost);
  CHECK(Busy(pending) && state.PublicationBytes() > 0 && fixture.Generations == 1 && held &&
            *held && std::ranges::equal((**held).Bytes(), bytes),
        "repair prepares the native replacement without deleting the existing row before commit");
  fixture.Release();
  const auto ready = fixture.Cache->Resolve(state, fixture.Factory());
  const auto after = (*store)->Load(fixture.Key, kBytesMost);
  CHECK(ready && !ready->Hit && state.PublicationBytes() == 0 && fixture.Generations == 1 &&
            before && *before && after && *after &&
            (**before).Record().Package != (**after).Record().Package,
        "successful atomic publication replaces the unsupported payload with one validated graph");
}

void RunningCancellation() {
  std::promise<void> entered;
  std::promise<void> release;
  auto running = entered.get_future();
  const auto gate = release.get_future().share();
  Fixture fixture;
  const bool opened = fixture.Open(false);
  CHECK(opened, "the running cancellation fixture opens");
  if (!opened) { return; }
  fixture.Start(gate, &entered);
  CHECK(running.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
        "the real native factory is executing when cancellation marks its worker");
  fixture.Job->Cancel();
  CHECK(!fixture.Job->Complete(),
        "running cancellation cannot report completion before owner cleanup");
  release.set_value();
  const auto deadline = Clock::now() + kDeadline;
  while (!fixture.Job->Complete() && Clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const auto result = fixture.Job->Collect();
  CHECK(result && !*result && result->error() == "street graph candidate canceled" &&
            fixture.SourceOwner.expired() && fixture.HeightOwner.expired() &&
            fixture.ResolverOwner.expired(),
        "a single cancel request makes active work terminal and releases captured native owners");
}

void PermanentFailure() {
  Fixture fixture;
  const bool opened = fixture.Open(false);
  CHECK(opened, "the permanent native storage fixture opens");
  if (!opened) { return; }
  CHECK(sqlite3_exec(
            fixture.Writer.get(),
            "CREATE TRIGGER reject_network BEFORE INSERT ON assets "
            "WHEN NEW.kind = 'street-network' BEGIN SELECT RAISE(ABORT, 'fixture failure'); END",
            nullptr,
            nullptr,
            nullptr) == SQLITE_OK,
        "a real permanent constraint rejects only native graph publication");
  Native::Resolution state(fixture.Key);
  const auto result = fixture.Cache->Resolve(state, fixture.Factory());
  CHECK(!result && !Busy(result) && state.PublicationAttempts() == 1 &&
            state.PublicationBytes() == 0 && fixture.Generated.expired() &&
            fixture.Generations == 1,
        "permanent storage errors terminate once and release graph/package owners");
}
}

int main() {
  RetainedCacheWrite();
  WorkerContinuation(false);
  WorkerContinuation(true);
  PermanentFailure();
  RunningCancellation();
  InvalidNativeReplacement();
  return Report();
}
