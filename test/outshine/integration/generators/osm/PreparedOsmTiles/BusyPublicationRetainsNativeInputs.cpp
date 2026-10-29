#include "AssetCache.h"
#include "Check.h"
#include "MvtWireFixture.h"
#include "OsmCacheFixture.h"
#include "PreparedOsmTiles.h"
#include "SourceSet.h"
#include "TilePool.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
using outshine::Test::OsmCache::NoTransport;
using outshine::Test::OsmCache::VectorSource;
constexpr auto kWait = std::chrono::milliseconds(1);
constexpr auto kDeadline = std::chrono::seconds(10);
constexpr LongitudeLatitude kEye{.LongitudeDeg = -45, .LatitudeDeg = 40};

struct Fixture {
  std::atomic_uint Calls{0};
  RawSourceCache Store{RawSourceCache::Config{.Directory = {}, .Using = RawSourceCache::Use::Off}};
  SourceSet Sources{Store};
  NoTransport Transport;
  Tasks Compute{2};
  std::unique_ptr<Ground::TilePool> Pool;
  std::shared_ptr<PreparedOsmTiles> Cache;
  std::unique_ptr<OsmField> Field;

  bool Open(const std::filesystem::path &root, PreparedOsmTiles::Budget budget, bool warm = false) {
    const auto registered = Sources.Add(std::make_unique<VectorSource>(
        "r1", warm ? std::vector<uint8_t>{} : Mvt::BuildingTile(), Calls));
    CHECK(registered == SourceSet::Registration::Accepted,
          "the actual vector fixture is registered without an HTTP transport");
    if (registered != SourceSet::Registration::Accepted) { return false; }
    Pool = std::make_unique<Ground::TilePool>(
        Ground::TilePool::Config{.Compute = &Compute, .PreparedFields = {}}, Sources, Transport);
    auto opened = PreparedOsmTiles::Open(root.string(), Sources, Compute, budget);
    CHECK(opened, "the native OSM cache opens with declared publication admission");
    if (!opened) { return false; }
    Cache = std::move(*opened);
    const std::array<std::string, 2> layers{"buildings", "water_polygons"};
    Field = std::make_unique<OsmField>(2, layers, MvtSchema::OpenMapTiles, Cache);
    return true;
  }

  bool Pump() {
    const bool okay = Field->Build(*Pool, kEye, 1, 9).has_value();
    CHECK(okay, "temporary writer contention leaves native tile work pending");
    (void)Compute.AwaitCompletion(std::chrono::duration<double>(kWait).count());
    return okay;
  }
};

using Writer = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

Writer OpenWriter(const std::filesystem::path &root) {
  sqlite3 *raw = nullptr;
  CHECK(sqlite3_open((root / "assets.sqlite").c_str(), &raw) == SQLITE_OK,
        "an independent connection opens the actual native store");
  return {raw, sqlite3_close};
}

bool Begin(sqlite3 *writer) {
  const bool held = sqlite3_exec(writer, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK;
  CHECK(held, "an independent writer holds publication admission");
  return held;
}

bool RetainWhileBusy(Fixture &fixture, bool constrained) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (std::chrono::steady_clock::now() < until) {
    if (!fixture.Pump()) { return false; }
    const auto cost = fixture.Cache->Costs();
    if (cost.PublicationDeferred > 0 && (constrained || cost.Generations == 9)) { break; }
  }
  const auto held = fixture.Cache->Costs();
  CHECK(held.PublicationDeferred > 0 && held.PublicationBytes > 0 && held.Writes == 0 &&
            !fixture.Field->SettledWithin(1),
        "busy native products stay retained and unavailable until their atomic commit");
  if (constrained) {
    CHECK(held.Generations > 0 && held.Generations <= 2,
          "capacity pressure admits only the already posted worker preparations");
  } else {
    CHECK(held.Generations == 9, "all nine source tiles are prepared once before retry");
  }
  const auto calls = fixture.Calls.load();
  for (unsigned turn = 0; turn < 32; ++turn) {
    if (!fixture.Pump()) { return false; }
  }
  CHECK(fixture.Cache->Costs().Generations == held.Generations && fixture.Calls == calls,
        "busy retries repeat neither source acquisition nor native generation");
  std::atomic_bool progressed{false};
  const auto other = fixture.Compute.Post([&progressed] { progressed = true; });
  CHECK(other != Tasks::kNoTask, "unrelated CPU work is admitted while publication waits");
  if (other != Tasks::kNoTask) { fixture.Compute.Wait(other); }
  CHECK(progressed, "the worker pool remains available under an external writer");
  return true;
}

void Resume(const std::filesystem::path &root, bool constrained) {
  Fixture fixture;
  const PreparedOsmTiles::Budget budget{
      .PendingMost = 128, .PublicationBytesMost = constrained ? 1 : size_t{128} * 1024 * 1024};
  if (!fixture.Open(root, budget)) { return; }
  auto writer = OpenWriter(root);
  if (!writer || !Begin(writer.get())) { return; }
  if (!RetainWhileBusy(fixture, constrained)) { return; }
  CHECK(sqlite3_exec(writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the independent writer releases its transaction");
  const auto resumed = std::chrono::steady_clock::now() + kDeadline;
  while (!fixture.Field->SettledWithin(1) && std::chrono::steady_clock::now() < resumed) {
    if (!fixture.Pump()) { return; }
  }
  const auto complete = fixture.Cache->Costs();
  CHECK(fixture.Field->SettledWithin(1) && fixture.Field->Tiles().size() == 9 &&
            fixture.Field->Features().size() == 9 && complete.Generations == 9 &&
            complete.Writes == 9 && complete.PublicationBytes == 0,
        "resumed publication supplies all original tiles and releases retained capacities");
  const std::vector<double> expected(fixture.Field->Points().begin(),
                                     fixture.Field->Points().end());
  Fixture warm;
  if (!warm.Open(root, {}, true) || !Begin(writer.get())) { return; }
  const auto reopened = std::chrono::steady_clock::now() + kDeadline;
  while (!warm.Field->SettledWithin(1) && std::chrono::steady_clock::now() < reopened) {
    if (!warm.Pump()) { return; }
  }
  CHECK(warm.Field->SettledWithin(1) && warm.Calls == 0 && warm.Cache->Costs().Hits == 9 &&
            warm.Cache->Costs().Generations == 0 &&
            std::ranges::equal(warm.Field->Points(), expected),
        "a fresh native owner restores exact geometry under another writer without its provider");
  CHECK(sqlite3_exec(writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the warm-hit writer releases its transaction");
}

void Cancel(const std::filesystem::path &root) {
  Fixture fixture;
  if (!fixture.Open(root, {})) { return; }
  auto writer = OpenWriter(root);
  if (!writer || !Begin(writer.get())) { return; }
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (fixture.Cache->Costs().PublicationDeferred == 0 &&
         std::chrono::steady_clock::now() < until) {
    if (!fixture.Pump()) { return; }
  }
  CHECK(fixture.Cache->Costs().PublicationBytes > 0,
        "cancellation starts with a real retained native publication");
  const std::weak_ptr<PreparedOsmTiles> observed = fixture.Cache;
  fixture.Field.reset();
  fixture.Cache.reset();
  CHECK(observed.expired(), "destroying the last consumer retires waiting native work");
  CHECK(sqlite3_exec(writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "writer admission is released after cancellation");
  sqlite3_stmt *rows = nullptr;
  CHECK(sqlite3_prepare_v2(writer.get(), "SELECT count(*) FROM assets", -1, &rows, nullptr) ==
            SQLITE_OK,
        "the independent owner inspects committed native rows");
  if (rows != nullptr) {
    CHECK(sqlite3_step(rows) == SQLITE_ROW && sqlite3_column_int64(rows, 0) == 0,
          "cancelled waiting work publishes no deferred product after its consumer disappears");
  }
  sqlite3_finalize(rows);
}

void RejectPermanentFailure(const std::filesystem::path &root) {
  Fixture fixture;
  if (!fixture.Open(root, {})) { return; }
  auto writer = OpenWriter(root);
  if (!writer) { return; }
  CHECK(sqlite3_exec(writer.get(),
                     "CREATE TRIGGER reject_native_write BEFORE INSERT ON assets "
                     "BEGIN SELECT RAISE(ABORT, 'fixture storage failure'); END",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "an actual storage constraint rejects publication without holding writer admission");
  bool failed = false;
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (std::chrono::steady_clock::now() < until) {
    const auto advanced = fixture.Field->Build(*fixture.Pool, kEye, 1, 9);
    if (!advanced) {
      failed = true;
      break;
    }
    (void)fixture.Compute.AwaitCompletion(std::chrono::duration<double>(kWait).count());
  }
  CHECK(failed && fixture.Cache->Costs().PublicationAttempts > 0 &&
            fixture.Cache->Costs().PublicationDeferred == 0 && fixture.Cache->Costs().Writes == 0 &&
            !fixture.Field->SettledWithin(1),
        "permanent storage failures remain errors and never become busy retries or ready data");
}
}

int main() {
  const std::filesystem::path root(PlantedPath("busy-native-osm-publication"));
  std::filesystem::create_directories(root);
  Resume(root / "ordinary", false);
  Resume(root / "bounded", true);
  Cancel(root / "cancelled");
  RejectPermanentFailure(root / "permanent");
  std::filesystem::remove_all(root);
  return Report();
}
