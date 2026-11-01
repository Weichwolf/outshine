#include "AssetCacheState.h"
#include <algorithm>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace outshine {
namespace {
using DatabaseHandle = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

struct Candidate {
  sqlite3 *Database = nullptr;
  std::string Key;
  int64_t Used = std::numeric_limits<int64_t>::max();
};

std::expected<int64_t, AssetCacheError> Scalar(sqlite3 *db, const char *sql) {
  AssetSql::Statement statement;
  if (const auto prepared = AssetSql::Prepare(db, sql, statement); !prepared) {
    return std::unexpected(prepared.error());
  }
  const int code = sqlite3_step(statement.Value);
  if (code != SQLITE_ROW) { return std::unexpected(AssetSql::Error(code)); }
  return sqlite3_column_int64(statement.Value, 0);
}

std::expected<void, AssetCacheError> Checkpoint(sqlite3 *db) {
  const int code =
      sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);
  if (code != SQLITE_OK) { return std::unexpected(AssetSql::Error(code)); }
  return {};
}

std::expected<void, AssetCacheError> CompactSlice(sqlite3 *db) {
  const auto mode = Scalar(db, "PRAGMA auto_vacuum");
  if (!mode) { return std::unexpected(mode.error()); }
  if (*mode != 2) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  const auto compacted = AssetSql::Exec(db, "PRAGMA incremental_vacuum(256)");
  if (!compacted) { return compacted; }
  return Checkpoint(db);
}

std::expected<std::optional<Candidate>, AssetCacheError> Oldest(sqlite3 *db) {
  AssetSql::Statement statement;
  const auto prepared = AssetSql::Prepare(
      db,
      "SELECT u.key,u.last_used_unix_s FROM cache_usage_v1 u INDEXED BY cache_usage_oldest "
      "WHERE EXISTS(SELECT 1 FROM packages p WHERE p.key=u.key) "
      "ORDER BY u.last_used_unix_s,u.key LIMIT 1",
      statement);
  if (!prepared) { return std::unexpected(prepared.error()); }
  const int step = sqlite3_step(statement.Value);
  if (step == SQLITE_DONE) { return std::optional<Candidate>{}; }
  if (step != SQLITE_ROW) { return std::unexpected(AssetSql::Error(step)); }
  const auto *key = reinterpret_cast<const char *>(sqlite3_column_text(statement.Value, 0));
  if (key == nullptr || !AssetSql::ValidKey(key) ||
      sqlite3_column_type(statement.Value, 1) != SQLITE_INTEGER ||
      sqlite3_column_int64(statement.Value, 1) < 0) {
    return std::unexpected(AssetCacheError::Storage);
  }
  return Candidate{.Database = db, .Key = key, .Used = sqlite3_column_int64(statement.Value, 1)};
}

std::expected<void, AssetCacheError> Erase(const Candidate &candidate) {
  sqlite3 *db = candidate.Database;
  const auto began = AssetSql::Exec(db, "BEGIN IMMEDIATE");
  if (!began) { return began; }

  struct Rollback {
    sqlite3 *Db;

    ~Rollback() {
      if (sqlite3_get_autocommit(Db) == 0) {
        (void)sqlite3_exec(Db, "ROLLBACK", nullptr, nullptr, nullptr);
      }
    }
  };

  const Rollback transaction{db};

  for (const char *sql : {"DELETE FROM bounds WHERE id IN(SELECT id FROM assets WHERE package=?)",
                          "DELETE FROM assets WHERE package=?",
                          "DELETE FROM package_bodies WHERE key=?",
                          "DELETE FROM packages WHERE key=?",
                          "DELETE FROM cache_usage_v1 WHERE key=?"}) {
    AssetSql::Statement statement;
    const auto prepared = AssetSql::Prepare(db, sql, statement);
    if (!prepared) { return prepared; }
    if (!AssetSql::Text(statement.Value, 1, candidate.Key)) {
      return std::unexpected(AssetCacheError::Storage);
    }
    const int code = sqlite3_step(statement.Value);
    if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
  }
  return AssetSql::Exec(db, "COMMIT");
}

std::expected<std::vector<DatabaseHandle>, AssetCacheError>
OpenBanks(const SqliteFileBudget &budget) {
  std::vector<DatabaseHandle> databases;
  std::error_code error;
  std::filesystem::directory_iterator files(budget.Directory(), error);
  if (error) { return std::unexpected(AssetCacheError::Storage); }
  for (const auto end = std::filesystem::directory_iterator(); files != end;
       files.increment(error)) {
    if (error) { return std::unexpected(AssetCacheError::Storage); }
    if (files->path().extension() != ".sqlite") { continue; }
    sqlite3 *raw = nullptr;
    const int opened = sqlite3_open_v2(files->path().string().c_str(),
                                       &raw,
                                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX,
                                       budget.VfsName());
    DatabaseHandle database(raw, sqlite3_close);
    if (opened != SQLITE_OK) { return std::unexpected(AssetSql::Error(opened)); }
    if (const auto setup = AssetSql::Exec(raw, "PRAGMA foreign_keys=ON;PRAGMA cache_size=-1024");
        !setup) {
      return std::unexpected(setup.error());
    }
    const auto version = Scalar(raw, "PRAGMA user_version");
    if (!version) { return std::unexpected(version.error()); }
    if (*version != 5) { continue; }
    databases.push_back(std::move(database));
  }
  if (error) { return std::unexpected(AssetCacheError::Storage); }
  return databases;
}

}

std::expected<void, AssetCacheError> AssetCache::State::Reclaim() const {
  const SqliteFileBudget::Maintenance maintenance(*Budget);
  if (Usage) {
    const auto flushed = Usage->Flush();
    if (!flushed) { return std::unexpected(AssetCacheError::Busy); }
  }
  auto databases = OpenBanks(*Budget);
  if (!databases) { return std::unexpected(databases.error()); }
  std::optional<Candidate> oldest;
  for (const auto &database : *databases) {
    sqlite3 *raw = database.get();
    const auto checkpoint = Checkpoint(raw);
    if (!checkpoint) { return checkpoint; }
    const auto free = Scalar(raw, "PRAGMA freelist_count");
    if (!free) { return std::unexpected(free.error()); }
    if (*free > 0) { return CompactSlice(raw); }
    const auto candidate = Oldest(raw);
    if (!candidate) { return std::unexpected(candidate.error()); }
    if (*candidate && (!oldest || (**candidate).Used < oldest->Used ||
                       ((**candidate).Used == oldest->Used && (**candidate).Key < oldest->Key))) {
      oldest = **candidate;
    }
  }
  if (!oldest) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  const auto erased = Erase(*oldest);
  if (!erased) { return erased; }
  return CompactSlice(oldest->Database);
}
}
