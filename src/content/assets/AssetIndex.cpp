#include "AssetIndexState.h"
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

AssetIndex::State::~State() {
  if (Database != nullptr) { sqlite3_close(Database); }
}

AssetIndex::AssetIndex(std::unique_ptr<State> state) : State_(std::move(state)) {}

AssetIndex::~AssetIndex() = default;

std::expected<std::unique_ptr<AssetIndex>, AssetIndexError>
AssetIndex::Open(const std::string &path) {
  if (path.empty() || path.contains('\0')) {
    return std::unexpected(AssetIndexError::InvalidInput);
  }
  auto state = std::make_unique<State>();
  const int opened =
      sqlite3_open_v2(path.c_str(),
                      &state->Database,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
                      nullptr);
  if (opened != SQLITE_OK) { return std::unexpected(AssetSql::Error(opened)); }
  AssetSql::Statement version;
  auto prepared = AssetSql::Prepare(state->Database, "PRAGMA user_version", version);
  if (!prepared) { return std::unexpected(prepared.error()); }
  const int step = sqlite3_step(version.Value);
  if (step != SQLITE_ROW) { return std::unexpected(AssetSql::Error(step)); }
  const int current = sqlite3_column_int(version.Value, 0);
  if (current != 0 && current != 1) { return std::unexpected(AssetIndexError::UnsupportedVersion); }
  sqlite3_reset(version.Value);
  const auto setup = AssetSql::Exec(
      state->Database,
      "PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA cache_size=-4096;"
      "BEGIN IMMEDIATE;"
      "CREATE TABLE IF NOT EXISTS assets(id INTEGER PRIMARY KEY,key TEXT NOT NULL UNIQUE,"
      "kind TEXT NOT NULL,minx REAL NOT NULL,miny REAL NOT NULL,minz REAL NOT NULL,"
      "maxx REAL NOT NULL,maxy REAL NOT NULL,maxz REAL NOT NULL,package TEXT NOT NULL,"
      "offset INTEGER NOT NULL,bytes INTEGER NOT NULL,level INTEGER NOT NULL,parent TEXT NOT NULL);"
      "CREATE VIRTUAL TABLE IF NOT EXISTS bounds USING rtree(id,minx,maxx,miny,maxy,minz,maxz);"
      "PRAGMA user_version=1;COMMIT;");
  if (!setup) { return std::unexpected(setup.error()); }
  return std::unique_ptr<AssetIndex>(new AssetIndex(std::move(state)));
}

namespace AssetSql {

Statement::~Statement() {
  if (Value != nullptr) { sqlite3_finalize(Value); }
}

AssetIndexError Error(int code) {
  return code == SQLITE_BUSY || code == SQLITE_LOCKED ? AssetIndexError::Busy
                                                      : AssetIndexError::Storage;
}

std::expected<void, AssetIndexError> Exec(sqlite3 *database, const char *sql) {
  const int code = sqlite3_exec(database, sql, nullptr, nullptr, nullptr);
  if (code != SQLITE_OK) { return std::unexpected(Error(code)); }
  return {};
}

std::expected<void, AssetIndexError>
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
         record.OffsetBytes <= most && record.ByteCount <= most - record.OffsetBytes;
}

}
}
