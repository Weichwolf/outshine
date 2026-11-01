#include "CacheTrimSql.h"
#include <utility>
#include <algorithm>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <limits>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>

namespace outshine::CacheTrimSql {

Database::~Database() {
  if (Transaction) { (void)sqlite3_exec(Handle, "ROLLBACK", nullptr, nullptr, nullptr); }
  sqlite3_close(Handle);
}

Statement::~Statement() {
  sqlite3_finalize(Handle);
}

bool Key(std::string_view key) {
  return key.size() == 64 && std::ranges::all_of(key, [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

std::expected<bool, std::string> Exists(const std::filesystem::path &path) {
  if (path.empty() || path.native().contains(std::filesystem::path::value_type{})) {
    return std::unexpected("cache path must be nonempty and contain no null characters");
  }
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (error == std::errc::no_such_file_or_directory || !std::filesystem::exists(status)) {
    if (!error || error == std::errc::no_such_file_or_directory) { return false; }
  }
  if (error) { return std::unexpected(path.string() + ": " + error.message()); }
  if (!std::filesystem::is_regular_file(status)) {
    return std::unexpected(path.string() + ": expected a regular cache file, not a symlink");
  }
  return true;
}

std::expected<void, std::string> Exec(sqlite3 *database, const char *sql) {
  if (sqlite3_exec(database, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
    return std::unexpected(sqlite3_errmsg(database));
  }
  return {};
}

std::expected<void, std::string> Prepare(sqlite3 *database, const char *sql, Statement &statement) {
  if (sqlite3_prepare_v2(database, sql, -1, &statement.Handle, nullptr) != SQLITE_OK) {
    return std::unexpected(sqlite3_errmsg(database));
  }
  return {};
}

namespace {
std::expected<int64_t, std::string> Scalar(sqlite3 *database, const char *sql) {
  Statement statement;
  if (auto prepared = Prepare(database, sql, statement); !prepared) {
    return std::unexpected(prepared.error());
  }
  if (sqlite3_step(statement.Handle) != SQLITE_ROW) {
    return std::unexpected(sqlite3_errmsg(database));
  }
  return sqlite3_column_int64(statement.Handle, 0);
}

std::expected<void, std::string> Checkpoint(sqlite3 *database) {
  const auto busy = Scalar(database, "PRAGMA wal_checkpoint(TRUNCATE)");
  if (!busy) { return std::unexpected("compaction checkpoint failed: " + busy.error()); }
  if (*busy != 0) { return std::unexpected("compaction checkpoint is busy or failed"); }
  return {};
}

std::expected<void, std::string> Compact(sqlite3 *database) {
  if (auto checkpointed = Checkpoint(database); !checkpointed) { return checkpointed; }
  const auto mode = Scalar(database, "PRAGMA auto_vacuum");
  if (!mode) { return std::unexpected(mode.error()); }
  if (*mode != 2) {
    if (auto vacuumed = Exec(database, "PRAGMA auto_vacuum=INCREMENTAL;VACUUM"); !vacuumed) {
      return vacuumed;
    }
    return Checkpoint(database);
  }
  auto free = Scalar(database, "PRAGMA freelist_count");
  if (!free) { return std::unexpected(free.error()); }
  while (*free > 0) {
    if (auto vacuumed = Exec(database, "PRAGMA incremental_vacuum(256)"); !vacuumed) {
      return vacuumed;
    }
    if (auto checkpointed = Checkpoint(database); !checkpointed) { return checkpointed; }
    const auto next = Scalar(database, "PRAGMA freelist_count");
    if (!next) { return std::unexpected(next.error()); }
    if (*next >= *free) { return std::unexpected("incremental compaction made no progress"); }
    free = next;
  }
  return {};
}
}

std::expected<void, std::string> Commit(Database &database, bool compact) {
  if (auto committed = Exec(database.Handle, "COMMIT"); !committed) { return committed; }
  database.Transaction = false;
  if (!compact) { return {}; }
  if (auto compacted = Compact(database.Handle); !compacted) {
    return std::unexpected("cache trimmed; compaction failed: " + compacted.error());
  }
  return {};
}

std::expected<std::unique_ptr<Database>, std::string>
Open(const std::filesystem::path &path, bool apply, std::shared_ptr<SqliteFileBudget> budget) {
  auto database = std::make_unique<Database>();
  database->Budget = std::move(budget);
  if (database->Budget && apply) {
    database->Maintenance = std::make_unique<SqliteFileBudget::Maintenance>(*database->Budget);
  }
  const int flags = apply ? (SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX)
                          : (SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX);
  if (sqlite3_open_v2(path.string().c_str(),
                      &database->Handle,
                      flags,
                      database->Budget ? database->Budget->VfsName() : nullptr) != SQLITE_OK) {
    return std::unexpected(path.string() + ": " + sqlite3_errmsg(database->Handle));
  }
  if (auto begun = Exec(database->Handle, apply ? "BEGIN IMMEDIATE" : "BEGIN"); !begun) {
    return std::unexpected(path.string() + ": " + begun.error());
  }
  database->Transaction = true;
  return database;
}

std::expected<bool, std::string> HasUsage(sqlite3 *database) {
  Statement query;
  auto prepared = Prepare(database,
                          "SELECT count(*) FROM sqlite_master WHERE type='table' "
                          "AND name='cache_usage_v1'",
                          query);
  if (!prepared) { return std::unexpected(prepared.error()); }
  if (sqlite3_step(query.Handle) != SQLITE_ROW) {
    return std::unexpected(sqlite3_errmsg(database));
  }
  return sqlite3_column_int64(query.Handle, 0) == 1;
}

std::expected<uint64_t, std::string> FileBytes(const std::filesystem::path &path) {
  uint64_t total = 0;
  for (const auto *suffix : {"", "-wal", "-shm"}) {
    const std::filesystem::path file = path.string() + suffix;
    auto exists = Exists(file);
    if (!exists) { return std::unexpected(exists.error()); }
    if (!*exists) { continue; }
    std::error_code error;
    const auto bytes = std::filesystem::file_size(file, error);
    if (error) { return std::unexpected(error.message()); }
    if (bytes > std::numeric_limits<uint64_t>::max() - total) {
      return std::unexpected("cache file byte count overflow");
    }
    total += bytes;
  }
  return total;
}

std::expected<std::unordered_map<std::string, int64_t>, std::string> Usage(sqlite3 *database) {
  auto present = HasUsage(database);
  if (!present) { return std::unexpected(present.error()); }
  std::unordered_map<std::string, int64_t> entries;
  if (!*present) { return entries; }
  Statement query;
  if (auto prepared = Prepare(database,
                              "SELECT key,stored_bytes,created_unix_s,last_used_unix_s "
                              "FROM cache_usage_v1",
                              query);
      !prepared) {
    return std::unexpected(prepared.error());
  }
  int step = SQLITE_OK;
  while ((step = sqlite3_step(query.Handle)) == SQLITE_ROW) {
    const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(query.Handle, 0));
    const int created = sqlite3_column_type(query.Handle, 2);
    if (text == nullptr || sqlite3_column_bytes(query.Handle, 0) != 64 || !Key(text) ||
        sqlite3_column_type(query.Handle, 1) != SQLITE_INTEGER ||
        sqlite3_column_int64(query.Handle, 1) < 0 ||
        (created != SQLITE_NULL &&
         (created != SQLITE_INTEGER || sqlite3_column_int64(query.Handle, 2) < 0)) ||
        sqlite3_column_type(query.Handle, 3) != SQLITE_INTEGER ||
        sqlite3_column_int64(query.Handle, 3) < 0) {
      return std::unexpected("invalid cache usage metadata");
    }
    entries.emplace(text, sqlite3_column_int64(query.Handle, 3));
  }
  if (step != SQLITE_DONE) { return std::unexpected(sqlite3_errmsg(database)); }
  return entries;
}

}
