#include "AssetCache.h"
#include "Check.h"
#include "Sha256.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <dlfcn.h>
#include <print>
#include <utility>
#include <span>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kUnrelated = 1024;
bool Observe = false;
int QuerySteps = 0;
size_t QueriesObserved = 0;

int Trace(unsigned event, void * /*context*/, void *statement, void * /*detail*/) {
  if (event != SQLITE_TRACE_PROFILE || !Observe) { return 0; }
  auto *query = static_cast<sqlite3_stmt *>(statement);
  const char *sql = sqlite3_sql(query);
  if (sql != nullptr && std::string_view(sql).starts_with("SELECT a.key,") &&
      std::string_view(sql).contains("a.parent=?")) {
    QuerySteps += sqlite3_stmt_status(query, SQLITE_STMTSTATUS_VM_STEP, 0);
    ++QueriesObserved;
  }
  return 0;
}

std::string Key(std::string_view value) {
  return Sha256Hex(value.data(), value.size());
}

void Queries(const AssetCache &cache,
             const Box &bounds,
             const std::array<AssetRecord, 8> &records) {
  AssetQuery query{.Bounds = bounds,
                   .Kind = {},
                   .Level = {},
                   .Radius = {},
                   .Planes = {},
                   .Parent = records[0].Key};
  auto children = cache.Select(query);
  CHECK(children && children->size() == 3,
        "Parent selects direct children across product kinds and LODs");
  query.Kind = "mesh";
  query.Level = 1;
  children = cache.Select(query);
  CHECK(children && children->size() == 1 && children->front() == records[1],
        "Parent, product kind and LOD select the exact native page metadata");
  query.Radius = AssetRadius{.Centre = {{4, 4, 4}}, .Metres = 1};
  children = cache.Select(query);
  CHECK(children && children->empty(), "Parent never bypasses the radius requirement");
  query.Radius.reset();
  const std::array<std::array<double, 4>, 1> planes{{{1, 0, 0, -2}}};
  query.Planes = planes;
  children = cache.Select(query);
  CHECK(children && children->empty(), "Parent never bypasses frustum rejection");
  query.Planes = {};
  query.Kind = {};
  query.Level.reset();
  query.Parent = "";
  children = cache.Select(query);
  CHECK(children && children->size() == 2, "an explicit empty Parent selects hierarchy roots");
  query.Parent.reset();
  children = cache.Select(query);
  CHECK(children && children->size() == records.size() + kUnrelated,
        "an absent Parent retains the ordinary spatial query");
  query.Parent = "not-an-asset-key";
  const auto invalid = cache.Select(query);
  CHECK(!invalid && invalid.error() == AssetCacheError::InvalidInput,
        "malformed parent addresses are rejected at the cache boundary");
}

void Unrelated(AssetCache &cache,
               const std::array<AssetRecord, 8> &records,
               std::span<const uint8_t> payload) {
  std::vector<AssetRecord> others(kUnrelated, records[1]);
  for (size_t at = 0; at < others.size(); ++at) {
    others[at].Key = Key("unrelated/" + std::to_string(at));
    others[at].Parent = Key("other-root");
  }
  CHECK(cache.Publish(others, payload),
        "many unrelated branches share the same spatial bounds and body");
}

void Indexed(const AssetCache &cache, const std::array<AssetRecord, 8> &records) {
  QuerySteps = 0;
  QueriesObserved = 0;
  Observe = true;
  const auto selected = cache.Select({.Bounds = records[0].Bounds,
                                      .Kind = "mesh",
                                      .Level = 1,
                                      .Radius = {},
                                      .Planes = {},
                                      .Parent = records[0].Key});
  Observe = false;
  CHECK(selected && selected->size() == 1 && selected->front() == records[1],
        "the native indexed query keeps its exact selected page");
  CHECK(QueriesObserved == 1 && QuerySteps > 0 && std::cmp_less(QuerySteps, kUnrelated),
        "native query work cannot include even one VM step per unrelated asset");
  std::println("ASSET_PARENT_QUERY unrelated={} matched={} vm_steps={}",
               kUnrelated,
               selected ? selected->size() : 0,
               QuerySteps);
}

void Exercise(const std::string &path) {
  auto opened = AssetCache::Open(path);
  CHECK(opened, "the renderer's native asset database opens");
  if (!opened) { return; }
  auto cache = std::move(*opened);
  std::array<uint8_t, 128> payload{};
  for (size_t at = 0; at < payload.size(); ++at) { payload[at] = static_cast<uint8_t>(at * 37u); }
  const Box bounds{.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}};
  std::array<AssetRecord, 8> records;
  for (size_t at = 0; at < records.size(); ++at) {
    records[at] = {.Key = Key(std::to_string(at)),
                   .Kind = "mesh",
                   .Bounds = bounds,
                   .Package = Sha256Hex(payload.data(), payload.size()),
                   .OffsetBytes = at * 16,
                   .ByteCount = 16,
                   .Level = 1,
                   .Parent = records[0].Key};
  }
  records[0].Parent.clear();
  records[2].Level = 2;
  records[3].Kind = "material-field";
  records[4].Parent = Key("other-root");
  records[5].Parent = records[1].Key;
  records[6].Parent = records[5].Key;
  records[7].Parent.clear();
  CHECK(cache->Publish(records, payload),
        "the database stores a multilevel native asset hierarchy");
  Unrelated(*cache, records, payload);
  Queries(*cache, bounds, records);
  Indexed(*cache, records);
  const auto ready = cache->Load(records[1].Key, payload.size());
  CHECK(ready && *ready, "the native page body is valid before the corruption control");
  cache.reset();
  sqlite3 *writer = nullptr;
  CHECK(sqlite3_open(path.c_str(), &writer) == SQLITE_OK,
        "the negative control opens independent storage");
  if (writer == nullptr) { return; }
  CHECK(sqlite3_exec(writer,
                     "UPDATE package_bodies SET bytes=zeroblob(length(bytes))",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK &&
            sqlite3_changes(writer) == 1,
        "the negative control corrupts the real stored payload without changing its metadata");
  CHECK(sqlite3_close(writer) == SQLITE_OK, "the independent control releases its database");
  opened = AssetCache::Open(path);
  CHECK(opened, "a fresh cache process can open the metadata independently of product bodies");
  if (!opened) { return; }
  Queries(**opened, bounds, records);
  Indexed(**opened, records);
  const auto corrupt = (*opened)->Load(records[1].Key, payload.size());
  CHECK(!corrupt || !*corrupt,
        "a real payload read detects the corruption that metadata queries never decode");
}
}

extern "C" int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs) {
  using Open = int (*)(const char *, sqlite3 **, int, const char *);
  static const auto open = reinterpret_cast<Open>(dlsym(RTLD_NEXT, "sqlite3_open_v2"));
  if (open == nullptr) { return SQLITE_CANTOPEN; }
  const int status = open(filename, ppDb, flags, zVfs);
  if (status == SQLITE_OK) { (void)sqlite3_trace_v2(*ppDb, SQLITE_TRACE_PROFILE, Trace, nullptr); }
  return status;
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-asset-parent-query-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  Exercise((directory / "assets.sqlite").string());
  std::filesystem::remove_all(directory);
  return outshine::Test::Report();
}
