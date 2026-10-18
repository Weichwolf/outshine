#include "ContentStore.h"
#include "content/AssetCache.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sqlite3.h>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Test;

int64_t Scalar(const std::filesystem::path &path, const char *sql) {
  sqlite3 *database = nullptr;
  CHECK(sqlite3_open_v2(path.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
        "independent read-only metadata observer opens");
  sqlite3_stmt *statement = nullptr;
  const int prepared = sqlite3_prepare_v2(database, sql, -1, &statement, nullptr);
  CHECK(prepared == SQLITE_OK, "native consumer metadata exists");
  int64_t result = -1;
  if (prepared == SQLITE_OK) {
    const int step = sqlite3_step(statement);
    CHECK(step == SQLITE_ROW, "metadata observer returns a row");
    if (step == SQLITE_ROW) { result = sqlite3_column_int64(statement, 0); }
  }
  (void)sqlite3_finalize(statement);
  CHECK(sqlite3_close(database) == SQLITE_OK, "metadata observer closes");
  return result;
}

void Sources(const std::filesystem::path &root) {
  std::filesystem::create_directories(root);
  const std::string legacy(64, 'a'), fresh(64, 'b');
  { std::ofstream(root / legacy) << "old"; }
  const std::array<uint8_t, 3> bytes{1, 2, 3};
  {
    Data::ContentStore store({.Directory = root.string()});
    CHECK(store.Keep(fresh, bytes.data(), bytes.size()), "new source is cached normally");
    CHECK(store.Read(fresh), "new source cache hit succeeds");
    CHECK(store.Read(legacy), "legacy source cache hit succeeds");
    CHECK(!store.Read(std::string(64, 'c')), "source miss remains a miss");
  }
  const auto index = root / ".usage.sqlite";
  CHECK(Scalar(index, "SELECT count(*) FROM cache_usage_v1") == 2,
        "only successful source writes and reads record usage");
  CHECK(Scalar(index, "SELECT sum(stored_bytes) FROM cache_usage_v1") == 6,
        "source metadata stores actual byte counts");
  CHECK(Scalar(index, "SELECT count(*) FROM cache_usage_v1 WHERE created_unix_s IS NULL") == 1,
        "legacy creation time stays unknown while new publication has a timestamp");
  CHECK(Scalar(index, "SELECT min(last_used_unix_s)>0 FROM cache_usage_v1") == 1,
        "source usage survives owner shutdown");
}

void Assets(const std::filesystem::path &path) {
  const std::array<uint8_t, 3> payload{7, 11, 13};
  const std::array<AssetRecord, 1> records{{{.Key = std::string(64, 'd'),
                                             .Kind = "usage-fixture",
                                             .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                             .ByteCount = payload.size()}}};
  {
    auto cache = AssetCache::Open(path.string());
    CHECK(cache, "native asset cache opens");
    if (!cache) { return; }
    CHECK((*cache)->Publish(records, payload), "native package publishes");
    auto loaded = (*cache)->Load(records[0].Key, 1024);
    CHECK(loaded && *loaded, "native package hit remains available");
  }
  CHECK(Scalar(path, "SELECT count(*) FROM cache_usage_v1") == 1,
        "asset publication and read share the package usage record");
  CHECK(Scalar(path,
               "SELECT count(*) FROM cache_usage_v1 u JOIN packages p ON p.key=u.key "
               "WHERE u.stored_bytes=length(p.bytes) AND u.created_unix_s>0 "
               "AND u.last_used_unix_s>=u.created_unix_s") == 1,
        "asset metadata retains actual compressed bytes and both timestamps");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-cache-consumer-usage-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  Sources(root / "sources");
  Assets(root / "assets.sqlite");
  std::filesystem::remove_all(root);
  return Report();
}
