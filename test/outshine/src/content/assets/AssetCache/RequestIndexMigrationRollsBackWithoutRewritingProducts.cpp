#include "AssetCache.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <span>
#include <sqlite3.h>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Test;
const std::string kProduct(64, 'a');
const std::string kRequest(64, 'b');
const std::string kAlias(64, 'c');

bool Execute(const std::string &path, const std::string &sql) {
  sqlite3 *database = nullptr;
  if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
    if (database != nullptr) { sqlite3_close(database); }
    return false;
  }
  const int result = sqlite3_exec(database, sql.c_str(), nullptr, nullptr, nullptr);
  return sqlite3_close(database) == SQLITE_OK && result == SQLITE_OK;
}

int64_t Scalar(const std::string &path, const char *sql) {
  sqlite3 *database = nullptr;
  if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
    if (database != nullptr) { sqlite3_close(database); }
    return -1;
  }
  sqlite3_stmt *statement = nullptr;
  int64_t result = -1;
  if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) == SQLITE_OK &&
      sqlite3_step(statement) == SQLITE_ROW) {
    result = sqlite3_column_int64(statement, 0);
  }
  sqlite3_finalize(statement);
  sqlite3_close(database);
  return result;
}

void Seed(const std::string &path) {
  auto cache = AssetCache::Open(path);
  CHECK(cache, "native product store opens");
  if (!cache) { return; }
  const std::array<uint8_t, 4> payload{17, 23, 31, 47};
  const std::array<AssetRecord, 1> records{{{.Key = kProduct,
                                             .Kind = "native",
                                             .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                             .Package = {},
                                             .ByteCount = payload.size(),
                                             .Parent = {},
                                             .RequestKey = kRequest}}};
  CHECK((*cache)->Publish(records, payload), "legacy product bytes and binding are seeded");
}

void Exercise(const std::string &path) {
  Seed(path);
  CHECK(
      Execute(path,
              "UPDATE packages SET bytes=(SELECT bytes FROM package_bodies WHERE key=packages.key);"
              "DROP TABLE package_bodies;"
              "ALTER TABLE assets ADD COLUMN request_key TEXT NOT NULL DEFAULT '';"
              "UPDATE assets SET request_key=COALESCE("
              "(SELECT request_key FROM asset_requests WHERE asset_id=assets.id),'');"
              "DROP TABLE asset_requests;"
              "CREATE UNIQUE INDEX asset_requests ON assets(request_key) WHERE request_key<>'';"
              "PRAGMA user_version=3;"
              "CREATE INDEX retain_old_request ON assets(request_key);"
              "CREATE TRIGGER no_package_insert BEFORE INSERT ON packages "
              "BEGIN SELECT RAISE(ABORT,'package insert'); END;"
              "CREATE TRIGGER no_package_update BEFORE UPDATE ON packages "
              "BEGIN SELECT RAISE(ABORT,'package update'); END;"
              "CREATE TRIGGER no_package_delete BEFORE DELETE ON packages "
              "BEGIN SELECT RAISE(ABORT,'package delete'); END;"),
      "version-three fixture retains its exact package and rejects all package writes");
  const auto refused = AssetCache::Open(path);
  CHECK(!refused && refused.error() == AssetCacheError::Storage,
        "schema dependency injects a migration failure after the request rows were copied");
  CHECK(Scalar(path, "PRAGMA user_version") == 3 &&
            Scalar(path,
                   "SELECT count(*) FROM pragma_table_info('assets') "
                   "WHERE name='request_key'") == 1 &&
            Scalar(path,
                   "SELECT count(*) FROM sqlite_master WHERE type='index' "
                   "AND name='asset_requests'") == 1 &&
            Scalar(path,
                   "SELECT count(*) FROM sqlite_master WHERE type='table' "
                   "AND name='asset_requests'") == 0,
        "failed migration atomically restores the old column, index and schema version");
  CHECK(Execute(path, "DROP INDEX retain_old_request"), "migration failure injector is removed");
  auto cache = AssetCache::Open(path);
  CHECK(cache, "the same legacy cache migrates without recreating product data");
  if (!cache) { return; }
  const auto old = (*cache)->LoadRequest(kRequest, 64);
  const auto alias = (*cache)->BindRequest(kProduct, kAlias);
  const auto loaded = (*cache)->LoadRequest(kAlias, 64);
  CHECK(old && *old && (**old).Record().RequestKey == kRequest && alias && *alias && loaded &&
            *loaded && (**loaded).Record().RequestKey == kAlias && (**old).Bytes().size() == 4 &&
            (**loaded).Bytes().back() == 47,
        "migrated original and added alias resolve the untouched native package");
  CHECK(Scalar(path, "PRAGMA user_version") == 5 &&
            Scalar(path,
                   "SELECT count(*) FROM pragma_table_info('assets') "
                   "WHERE name='request_key'") == 0 &&
            Scalar(path, "SELECT count(*) FROM asset_requests") == 2 &&
            Scalar(path, "SELECT count(*) FROM packages") == 1,
        "only the normalized request index remains authoritative");
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-request-migration-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  Exercise((directory / "assets.sqlite").string());
  std::filesystem::remove_all(directory);
  return Report();
}
