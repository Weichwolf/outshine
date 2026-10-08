#include "AssetCacheState.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace outshine {

AssetCache::State::~State() {
  if (Database != nullptr) { sqlite3_close(Database); }
}

AssetCache::AssetCache(std::unique_ptr<State> state) : State_(std::move(state)) {}

AssetCache::~AssetCache() = default;

std::expected<std::unique_ptr<AssetCache>, AssetCacheError>
AssetCache::Open(const std::string &path) {
  if (path.empty() || path.contains('\0')) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  auto state = std::make_unique<State>();
  const int opened =
      sqlite3_open_v2(path.c_str(),
                      &state->Database,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
                      nullptr);
  if (opened != SQLITE_OK) { return std::unexpected(AssetSql::Error(opened)); }
  const auto began =
      AssetSql::Exec(state->Database,
                     "PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA cache_size=-4096;"
                     "BEGIN IMMEDIATE;");
  if (!began) { return std::unexpected(began.error()); }
  AssetSql::Statement version;
  auto prepared = AssetSql::Prepare(state->Database, "PRAGMA user_version", version);
  if (!prepared) { return std::unexpected(prepared.error()); }
  const int step = sqlite3_step(version.Value);
  if (step != SQLITE_ROW) { return std::unexpected(AssetSql::Error(step)); }
  const int current = sqlite3_column_int(version.Value, 0);
  if (current < 0 || current > 3) { return std::unexpected(AssetCacheError::UnsupportedVersion); }
  sqlite3_reset(version.Value);
  const auto setup = AssetSql::Exec(
      state->Database,
      "CREATE TABLE IF NOT EXISTS assets(id INTEGER PRIMARY KEY,key TEXT NOT NULL UNIQUE,"
      "kind TEXT NOT NULL,minx REAL NOT NULL,miny REAL NOT NULL,minz REAL NOT NULL,"
      "maxx REAL NOT NULL,maxy REAL NOT NULL,maxz REAL NOT NULL,package TEXT NOT NULL,"
      "offset INTEGER NOT NULL,bytes INTEGER NOT NULL,level INTEGER NOT NULL,parent TEXT NOT NULL);"
      "CREATE VIRTUAL TABLE IF NOT EXISTS bounds USING rtree(id,minx,maxx,miny,maxy,minz,maxz);"
      "CREATE TABLE IF NOT EXISTS packages(key TEXT PRIMARY KEY,bytes BLOB NOT NULL,crc INTEGER "
      "NOT NULL);");
  if (!setup) { return std::unexpected(setup.error()); }
  if (current < 2) {
    const auto migrated =
        AssetSql::Exec(state->Database,
                       "ALTER TABLE packages ADD COLUMN codec INTEGER NOT NULL DEFAULT 0;"
                       "ALTER TABLE packages ADD COLUMN native_bytes INTEGER NOT NULL DEFAULT 0;");
    if (!migrated) { return std::unexpected(migrated.error()); }
  }
  if (current < 3) {
    const auto migrated = AssetSql::Exec(
        state->Database, "ALTER TABLE assets ADD COLUMN request_key TEXT NOT NULL DEFAULT '';");
    if (!migrated) { return std::unexpected(migrated.error()); }
  }
  const auto committed =
      AssetSql::Exec(state->Database,
                     "CREATE UNIQUE INDEX IF NOT EXISTS asset_requests ON assets(request_key) "
                     "WHERE request_key<>'';PRAGMA user_version=3;COMMIT;");
  if (!committed) { return std::unexpected(committed.error()); }
  return std::unique_ptr<AssetCache>(new AssetCache(std::move(state)));
}

namespace AssetSql {

Statement::~Statement() {
  if (Value != nullptr) { sqlite3_finalize(Value); }
}

AssetCacheError Error(int code) {
  return code == SQLITE_BUSY || code == SQLITE_LOCKED ? AssetCacheError::Busy
                                                      : AssetCacheError::Storage;
}

std::expected<void, AssetCacheError> Exec(sqlite3 *database, const char *sql) {
  const int code = sqlite3_exec(database, sql, nullptr, nullptr, nullptr);
  if (code != SQLITE_OK) { return std::unexpected(Error(code)); }
  return {};
}

std::expected<void, AssetCacheError>
Prepare(sqlite3 *database, const char *sql, Statement &statement) {
  const int code = sqlite3_prepare_v2(database, sql, -1, &statement.Value, nullptr);
  if (code != SQLITE_OK) { return std::unexpected(Error(code)); }
  return {};
}

bool Text(sqlite3_stmt *statement, int slot, std::string_view value) {
  if (value.size() > static_cast<size_t>(std::numeric_limits<int>::max())) { return false; }
  return sqlite3_bind_text(statement,
                           slot,
                           value.empty() ? "" : value.data(),
                           static_cast<int>(value.size()),
                           SQLITE_TRANSIENT) == SQLITE_OK;
}

bool Valid(const Box &bounds) {
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(bounds.Min[axis]) || !std::isfinite(bounds.Max[axis]) ||
        bounds.Min[axis] > bounds.Max[axis] ||
        std::abs(bounds.Min[axis]) > std::numeric_limits<float>::max() ||
        std::abs(bounds.Max[axis]) > std::numeric_limits<float>::max()) {
      return false;
    }
  }
  return true;
}

bool ValidKey(std::string_view key) {
  return key.size() == 64 && std::ranges::all_of(key, [](char value) {
           return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
         });
}

bool Valid(const AssetRecord &record) {
  constexpr auto most = static_cast<uint64_t>(std::numeric_limits<sqlite3_int64>::max());
  return ValidKey(record.Key) && !record.Kind.empty() && Valid(record.Bounds) &&
         ValidKey(record.Package) && (record.Parent.empty() || ValidKey(record.Parent)) &&
         (!record.RequestKey || ValidKey(*record.RequestKey)) && record.OffsetBytes <= most &&
         record.ByteCount <= most - record.OffsetBytes;
}

}
}
