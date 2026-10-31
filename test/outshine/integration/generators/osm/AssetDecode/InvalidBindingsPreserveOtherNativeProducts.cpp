#include "AssetCache.h"
#include "Check.h"
#include "MvtWireFixture.h"
#include "OsmCacheFixture.h"
#include "PreparedOsmTiles.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using Product = std::shared_ptr<const OsmField>;
constexpr TileAt kFirst{.X = 1, .Y = 1};
constexpr TileAt kSecond{.X = 2, .Y = 1};
constexpr size_t kBytesMost = size_t{64} * 1024 * 1024;

Product Finish(PreparedOsmTiles &cache, Tasks &compute, const OsmField &owner, TileAt at) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < until) {
    auto next = cache.Acquire(at, owner);
    CHECK(next, "the real native owner advances while cache bindings change");
    if (!next) { return {}; }
    if (next->Status == PreparedOsmTiles::State::Ready) { return std::move(next->Product); }
    (void)compute.AwaitCompletion(0.001);
  }
  return {};
}

std::array<std::string, 2> Keys(AssetCache &store) {
  std::array<std::string, 2> keys;
  auto rows = store.Select({.Bounds = {.Min = {{-1e9, -1e9, -1e9}}, .Max = {{1e9, 1e9, 1e9}}},
                            .Kind = "osm-tile",
                            .Level = 2,
                            .Radius = std::nullopt,
                            .Planes = {}});
  CHECK(rows && rows->size() == 2, "two real generated native contents are persisted");
  if (!rows) { return keys; }
  for (const auto &row : *rows) {
    auto loaded = store.Load(row.Key, kBytesMost);
    auto field =
        loaded && *loaded ? OsmField::DecodeNativeTile((**loaded).Bytes(), kBytesMost) : nullptr;
    CHECK(field && field->Tiles().size() == 1, "each inspected package is valid native geometry");
    if (!field) { continue; }
    const auto x = field->Tiles().front().X;
    if (x == 1 || x == 2) { keys[static_cast<size_t>(x - 1)] = row.Key; }
  }
  return keys;
}

std::string Request(const std::filesystem::path &database, const std::string &key) {
  sqlite3 *raw = nullptr;
  const auto opened = sqlite3_open_v2(database.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
  if (opened != SQLITE_OK) { return {}; }
  sqlite3_stmt *query = nullptr;
  const auto prepared = sqlite3_prepare_v2(
      raw,
      "SELECT request_key FROM asset_requests WHERE asset_id=(SELECT id FROM assets WHERE key=?)",
      -1,
      &query,
      nullptr);
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(query,
                                                                             sqlite3_finalize);
  if (prepared != SQLITE_OK ||
      sqlite3_bind_text(query, 1, key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
      sqlite3_step(query) != SQLITE_ROW || sqlite3_column_type(query, 0) != SQLITE_TEXT) {
    return {};
  }
  return reinterpret_cast<const char *>(sqlite3_column_text(query, 0));
}

void Check(const std::filesystem::path &directory) {
  std::atomic_uint calls{0};
  Test::OsmCache::NoTransport transport;
  Tasks compute(2);
  auto session = Test::OsmCache::OpenSession(
      std::make_unique<Test::OsmCache::VectorSource>("r1", Test::Mvt::BuildingTile(), calls));
  CHECK(session, "the actual provider session opens");
  if (!session) { return; }
  auto service = PreparedOsmTiles::Open(directory.string(), *session, transport, compute);
  CHECK(service, "native preparation opens its real asset pipeline");
  if (!service) { return; }
  const std::array<std::string, 1> layers{"buildings"};
  const OsmField seedA(2, layers, MvtSchema::OpenMapTiles);
  const OsmField seedB(2, layers, MvtSchema::OpenMapTiles);
  const auto a = Finish(**service, compute, seedA, kFirst);
  const auto b = Finish(**service, compute, seedB, kSecond);
  CHECK(a && b && calls == 2,
        "both distinct native tiles have genuine source and generation evidence");
  if (!a || !b) { return; }
  const auto database = directory / "assets.sqlite";
  auto store = AssetCache::Open(database.string());
  CHECK(store, "an independent native-store connection opens");
  if (!store) { return; }
  const auto keys = Keys(**store);
  const auto firstRequest = Request(database, keys[0]);
  const auto secondRequest = Request(database, keys[1]);
  CHECK(!firstRequest.empty() && !secondRequest.empty() && firstRequest != secondRequest,
        "both native request addresses come from their actual committed bindings");
  if (firstRequest.empty() || secondRequest.empty()) { return; }
  CHECK((*store)->BindRequest(keys[1], firstRequest),
        "the first request is deliberately pointed at another valid native tile");
  const OsmField current(2, layers, MvtSchema::OpenMapTiles);
  const OsmField idle(2, layers, MvtSchema::OpenMapTiles);
  CHECK((*service)->Acquire(kFirst, current) && (*service)->Acquire(kFirst, idle),
        "two real consumers join before the wrong native result is rejected");
  const auto recovered = Finish(**service, compute, current, kFirst);
  CHECK(recovered && recovered->Tiles().front().X == kFirst.X,
        "a current consumer repairs its request while another joined ticket remains idle");
  const auto foreign = (*store)->Find(keys[1]);
  const auto firstBinding = (*store)->FindRequest(firstRequest);
  const auto secondBinding = (*store)->FindRequest(secondRequest);
  CHECK(foreign && *foreign && firstBinding && *firstBinding && secondBinding && *secondBinding &&
            (**firstBinding).Key == keys[0] && (**secondBinding).Key == keys[1] &&
            b->Tiles().front().X == kSecond.X,
        "request repair preserves the unrelated native product, binding and live owner");
  const auto resumed = Finish(**service, compute, idle, kFirst);
  CHECK(resumed && resumed->Tiles().front().X == kFirst.X && calls == 3 &&
            (*service)->Costs().Generations == 2,
        "the idle consumer re-resolves without repeating either native generation");
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-native-binding-repair-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Check(directory);
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the isolated cache is removed after every owner and worker retires");
  return outshine::Test::Report();
}
