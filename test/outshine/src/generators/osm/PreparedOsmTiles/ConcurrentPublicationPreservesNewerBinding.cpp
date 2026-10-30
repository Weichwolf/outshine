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
#include <dlfcn.h>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr LongitudeLatitude kEye{.LongitudeDeg = -45, .LatitudeDeg = 40};
constexpr size_t kBytesMost = size_t{1024} * 1024;
constexpr auto kDeadline = std::chrono::seconds(10);

struct Fixture {
  std::atomic_uint Calls{0};
  std::shared_ptr<SourceSession> Session;
  OsmCache::NoTransport Wire;
  Tasks Compute{1};
  std::unique_ptr<Ground::TilePool> Pool;
  std::shared_ptr<PreparedOsmTiles> Cache;
  std::unique_ptr<OsmField> Field;

  bool Open(const std::filesystem::path &root, std::vector<uint8_t> input) {
    auto session = OsmCache::OpenSession(
        std::make_unique<OsmCache::VectorSource>("r1", std::move(input), Calls));
    if (!session) { return false; }
    Session = std::move(*session);
    Pool = std::make_unique<Ground::TilePool>(
        Ground::TilePool::Config{.Compute = &Compute, .PreparedFields = {}},
        Session->Sources(),
        Wire);
    auto cache = PreparedOsmTiles::Open(root.string(), Session, Wire, Compute);
    if (!cache) { return false; }
    Cache = std::move(*cache);
    const std::array<std::string, 2> layers{"buildings", "water_polygons"};
    Field = std::make_unique<OsmField>(2, layers, MvtSchema::OpenMapTiles, Cache);
    return true;
  }

  bool Poll() {
    const auto at = OsmField::Locate(kEye, 2);
    if (!at) { return false; }
    const auto result = Field->PollTile(*Pool, *at);
    (void)Compute.AwaitCompletion(0.001);
    return result && *result;
  }

  bool Finish() {
    const auto until = std::chrono::steady_clock::now() + kDeadline;
    while (std::chrono::steady_clock::now() < until) {
      if (Poll()) { return true; }
    }
    return false;
  }

  bool WaitForBusyPublication() {
    const auto until = std::chrono::steady_clock::now() + kDeadline;
    while (std::chrono::steady_clock::now() < until) {
      (void)Poll();
      if (Cache->Costs().PublicationDeferred > 0 && Cache->Costs().PublicationBytes > 0) {
        return true;
      }
    }
    return false;
  }
};

std::expected<std::string, std::string> SingleRequest(const std::filesystem::path &root) {
  sqlite3 *raw = nullptr;
  if (sqlite3_open((root / "assets.sqlite").c_str(), &raw) != SQLITE_OK) {
    if (raw != nullptr) { (void)sqlite3_close(raw); }
    return std::unexpected("rival native store did not open");
  }
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
  sqlite3_stmt *statement = nullptr;
  if (sqlite3_prepare_v2(raw, "SELECT request_key FROM asset_requests", -1, &statement, nullptr) !=
      SQLITE_OK) {
    return std::unexpected("rival request table was unavailable");
  }
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> row(statement, sqlite3_finalize);
  if (sqlite3_step(statement) != SQLITE_ROW || sqlite3_column_text(statement, 0) == nullptr) {
    return std::unexpected("the actual rival request was not published");
  }
  std::string request(reinterpret_cast<const char *>(sqlite3_column_text(statement, 0)));
  if (sqlite3_step(statement) != SQLITE_DONE) {
    return std::unexpected("the counterexample expected one native request");
  }
  return request;
}

struct Hook;
Hook *ActiveHook = nullptr;

struct Hook {
  enum class Action { PublishRival, EvictCommittedContent };
  std::string Path;
  std::unique_ptr<AssetCache> Writer;
  CachedAsset Packet;
  AssetRecord Record;
  std::atomic_bool Armed{false};
  std::atomic_uint Fires{0};
  std::atomic_bool Published{false};
  Action Then = Action::PublishRival;

  Hook(std::string path, std::unique_ptr<AssetCache> writer, CachedAsset packet)
      : Path(std::move(path)),
        Writer(std::move(writer)),
        Packet(std::move(packet)),
        Record(Packet.Record()) {
    Record.Package.clear();
    Record.OffsetBytes = 0;
    Record.ByteCount = Packet.Bytes().size();
    ActiveHook = this;
  }

  ~Hook() { ActiveHook = nullptr; }

  Hook(const Hook &) = delete;
  Hook &operator=(const Hook &) = delete;
};

int Trace(unsigned event, void * /*context*/, void *statement, void * /*detail*/) {
  if (ActiveHook == nullptr) { return 0; }
  auto &hook = *ActiveHook;
  const char *sql = sqlite3_sql(static_cast<sqlite3_stmt *>(statement));
  if (sql == nullptr) { return 0; }
  const bool boundary =
      hook.Then == Hook::Action::PublishRival
          ? event == SQLITE_TRACE_STMT && std::string_view(sql) == "BEGIN IMMEDIATE"
          : event == SQLITE_TRACE_PROFILE && std::string_view(sql) == "COMMIT";
  if (!boundary || !hook.Armed.exchange(false)) { return 0; }
  ++hook.Fires;
  if (hook.Then == Hook::Action::PublishRival) {
    hook.Published =
        hook.Writer->Publish(std::span(&hook.Record, 1), hook.Packet.Bytes()).has_value();
  } else {
    const auto found = hook.Writer->Find(hook.Record.Key);
    hook.Published = found && *found && hook.Writer->Remove(hook.Record.Key).has_value();
  }
  return 0;
}

void CheckRace(const std::filesystem::path &root) {
  Fixture reference;
  auto revised = Mvt::BuildingTile();
  revised.insert(revised.end(), {0x78, 1});
  CHECK(reference.Open(root / "reference", std::move(revised)) && reference.Finish(),
        "a real revised MVT produces one complete native rival tile");
  if (!reference.Field || reference.Field->Tiles().size() != 1) { return; }
  const auto request = SingleRequest(root / "reference");
  auto source = AssetCache::Open((root / "reference/assets.sqlite").string());
  CHECK(request && source, "the actual rival native binding and store are available");
  if (!request || !source) { return; }
  auto packet = (*source)->LoadRequest(*request, kBytesMost);
  CHECK(packet && *packet, "the rival owns its genuine generated native package");
  if (!packet || !*packet) { return; }
  const std::string expectedKey = (**packet).Record().Key;
  const std::string expectedInput = reference.Field->Tiles().front().InputDigest;
  std::filesystem::create_directories(root / "target");
  auto writer = AssetCache::Open((root / "target/assets.sqlite").string());
  CHECK(writer.has_value(), "an independent rival connection opens the target store");
  if (!writer) { return; }
  Hook hook((root / "target/assets.sqlite").string(), std::move(*writer), std::move(**packet));
  Fixture target;
  CHECK(target.Open(root / "target", Mvt::BuildingTile()), "the original native producer opens");
  if (!target.Cache) { return; }
  sqlite3 *raw = nullptr;
  CHECK(sqlite3_open(hook.Path.c_str(), &raw) == SQLITE_OK, "a separate writer gate opens");
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> gate(raw, sqlite3_close);
  if (!gate) { return; }
  CHECK(sqlite3_exec(raw, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "writer admission stays occupied until the original package is prepared");
  CHECK(target.WaitForBusyPublication(), "the original package survives actual writer contention");
  CHECK(sqlite3_exec(raw, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the writer gate releases before the adversarial native commit");
  hook.Armed = true;
  CHECK(target.Finish(), "the original native consumer eventually completes");
  CHECK(hook.Fires == 1 && hook.Published,
        "the rival really commits between the missing lookup and original writer admission");
  const auto current = hook.Writer->FindRequest(*request);
  CHECK(current && *current && (**current).Key == expectedKey,
        "the late original publication preserves the newer committed request target");
  const auto contents =
      hook.Writer->Select({.Bounds = {.Min = {{-1e9, -1e9, -1e9}}, .Max = {{1e9, 1e9, 1e9}}},
                           .Kind = "osm-tile",
                           .Level = 2,
                           .Radius = std::nullopt,
                           .Planes = {}});
  CHECK(contents && contents->size() == 2,
        "both independently committed native contents survive the request-binding race");
  CHECK(target.Field->Tiles().size() == 1 &&
            target.Field->Tiles().front().InputDigest == expectedInput &&
            std::ranges::equal(target.Field->Points(), reference.Field->Points()),
        "the live consumer receives the winning native input and complete original geometry");
  CHECK(target.Calls == 1 && target.Cache->Costs().Generations == 1 &&
            target.Cache->Costs().PublicationBytes == 0,
        "the race repeats neither source acquisition nor normalization and releases its package");
  Fixture warm;
  CHECK(warm.Open(root / "target", {}) && warm.Finish(),
        "a fresh owner replays the persisted native winner without source data");
  CHECK(warm.Calls == 0 && warm.Field->Tiles().size() == 1 &&
            warm.Field->Tiles().front().InputDigest == expectedInput,
        "the warm path retains the winning binding rather than restoring its obsolete predecessor");
}

void CheckEvictionBeforeBinding(const std::filesystem::path &root) {
  Fixture reference;
  CHECK(reference.Open(root / "reference", Mvt::BuildingTile()) && reference.Finish(),
        "the eviction control produces a genuine native reference");
  const auto request = SingleRequest(root / "reference");
  auto source = AssetCache::Open((root / "reference/assets.sqlite").string());
  CHECK(request && source, "the eviction control has its actual stored request");
  if (!request || !source) { return; }
  auto packet = (*source)->LoadRequest(*request, kBytesMost);
  CHECK(packet && *packet, "the eviction control obtains the real content identity");
  if (!packet || !*packet) { return; }
  std::filesystem::create_directories(root / "target");
  auto writer = AssetCache::Open((root / "target/assets.sqlite").string());
  if (!writer) {
    CHECK(false, "the eviction writer opens");
    return;
  }
  Hook hook((root / "target/assets.sqlite").string(), std::move(*writer), std::move(**packet));
  hook.Then = Hook::Action::EvictCommittedContent;
  Fixture target;
  CHECK(target.Open(root / "target", Mvt::BuildingTile()), "the eviction target opens");
  if (!target.Cache) { return; }
  sqlite3 *raw = nullptr;
  CHECK(sqlite3_open(hook.Path.c_str(), &raw) == SQLITE_OK, "the eviction writer gate opens");
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> gate(raw, sqlite3_close);
  if (!gate) { return; }
  CHECK(sqlite3_exec(raw, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK &&
            target.WaitForBusyPublication(),
        "the eviction target retains one complete prepared package");
  CHECK(sqlite3_exec(raw, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the eviction writer gate releases");
  hook.Armed = true;
  CHECK(target.Finish(), "eviction between content commit and binding does not strand the request");
  CHECK(hook.Fires == 1 && hook.Published && target.Cache->Costs().Writes == 2,
        "one actual committed row is evicted and the retained package is committed again");
  const auto current = hook.Writer->FindRequest(*request);
  CHECK(current && *current && (**current).Key == hook.Record.Key && target.Calls == 1 &&
            target.Cache->Costs().Generations == 1 && target.Cache->Costs().PublicationBytes == 0,
        "recovery keeps the original content identity without repeating source or normalization");
}
}

extern "C" int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs) {
  using Open = int (*)(const char *, sqlite3 **, int, const char *);
  static const auto open = reinterpret_cast<Open>(dlsym(RTLD_NEXT, "sqlite3_open_v2"));
  if (open == nullptr) { return SQLITE_CANTOPEN; }
  const int status = open(filename, ppDb, flags, zVfs);
  if (status == SQLITE_OK && ActiveHook != nullptr && filename != nullptr &&
      ActiveHook->Path == filename) {
    (void)sqlite3_trace_v2(*ppDb, SQLITE_TRACE_STMT | SQLITE_TRACE_PROFILE, Trace, nullptr);
  }
  return status;
}

int main() {
  const std::filesystem::path root(PlantedPath("native-osm-publication-binding"));
  std::filesystem::create_directories(root);
  CheckRace(root / "rival");
  CheckEvictionBeforeBinding(root / "eviction");
  std::filesystem::remove_all(root);
  return Report();
}
