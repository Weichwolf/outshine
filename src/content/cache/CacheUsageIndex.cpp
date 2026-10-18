#include "CacheUsageIndex.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace outshine {
namespace {
constexpr size_t kBatchEntries = 256;

struct Observation {
  uint64_t Bytes = 0;
  std::optional<int64_t> Created;
  int64_t Used = 0;
};

struct Statement {
  sqlite3_stmt *Handle = nullptr;

  ~Statement() { sqlite3_finalize(Handle); }
};

std::string Error(sqlite3 *database) {
  return std::string("cache usage index: ") + sqlite3_errmsg(database);
}

std::expected<void, std::string>
Write(sqlite3 *database, const std::unordered_map<std::string, Observation> &pending) {
  if (pending.empty()) { return {}; }
  if (sqlite3_exec(database, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) != SQLITE_OK) {
    return std::unexpected(Error(database));
  }
  Statement statement;
  int code = sqlite3_prepare_v2(
      database,
      "INSERT INTO cache_usage_v1(key,stored_bytes,created_unix_s,last_used_unix_s) "
      "VALUES(?,?,?,?) ON CONFLICT(key) DO UPDATE SET stored_bytes=excluded.stored_bytes,"
      "created_unix_s=coalesce(cache_usage_v1.created_unix_s,excluded.created_unix_s),"
      "last_used_unix_s=max(cache_usage_v1.last_used_unix_s,excluded.last_used_unix_s) "
      "WHERE cache_usage_v1.stored_bytes<>excluded.stored_bytes OR "
      "excluded.last_used_unix_s-cache_usage_v1.last_used_unix_s>=60 OR "
      "(cache_usage_v1.created_unix_s IS NULL AND excluded.created_unix_s IS NOT NULL)",
      -1,
      &statement.Handle,
      nullptr);
  for (const auto &[key, observation] : pending) {
    if (code != SQLITE_OK) { break; }
    code = sqlite3_bind_text(
        statement.Handle, 1, key.data(), static_cast<int>(key.size()), SQLITE_TRANSIENT);
    if (code == SQLITE_OK) {
      code = sqlite3_bind_int64(statement.Handle, 2, static_cast<int64_t>(observation.Bytes));
    }
    if (code == SQLITE_OK) {
      code = observation.Created ? sqlite3_bind_int64(statement.Handle, 3, *observation.Created)
                                 : sqlite3_bind_null(statement.Handle, 3);
    }
    if (code == SQLITE_OK) { code = sqlite3_bind_int64(statement.Handle, 4, observation.Used); }
    if (code == SQLITE_OK) {
      const int step = sqlite3_step(statement.Handle);
      code = step == SQLITE_DONE ? SQLITE_OK : step;
    }
    if (code == SQLITE_OK) { code = sqlite3_reset(statement.Handle); }
    if (code == SQLITE_OK) { code = sqlite3_clear_bindings(statement.Handle); }
  }
  if (code == SQLITE_OK) { code = sqlite3_exec(database, "COMMIT", nullptr, nullptr, nullptr); }
  if (code == SQLITE_OK) { return {}; }
  auto error = Error(database);
  (void)sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
  return std::unexpected(std::move(error));
}
}

struct CacheUsageIndex::State {
  sqlite3 *Database = nullptr;
  std::mutex Mutex;
  std::unordered_map<std::string, Observation> Pending;

  ~State() { sqlite3_close(Database); }
};

CacheUsageIndex::CacheUsageIndex(std::unique_ptr<State> state) : State_(std::move(state)) {}

CacheUsageIndex::~CacheUsageIndex() {
  if (!Flush()) { std::fputs("cache usage index: pending observations were not saved\n", stderr); }
}

std::expected<std::unique_ptr<CacheUsageIndex>, std::string>
CacheUsageIndex::Open(const std::string &path) {
  if (path.empty() || path.contains('\0')) {
    return std::unexpected("cache usage index requires a valid path");
  }
  auto state = std::make_unique<State>();
  if (sqlite3_open_v2(path.c_str(),
                      &state->Database,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
                      nullptr) != SQLITE_OK) {
    return std::unexpected(Error(state->Database));
  }
  if (sqlite3_exec(state->Database,
                   "PRAGMA journal_mode=WAL;PRAGMA synchronous=NORMAL;"
                   "CREATE TABLE IF NOT EXISTS cache_usage_v1(key TEXT PRIMARY KEY,"
                   "stored_bytes INTEGER NOT NULL,created_unix_s INTEGER,"
                   "last_used_unix_s INTEGER NOT NULL)",
                   nullptr,
                   nullptr,
                   nullptr) != SQLITE_OK) {
    return std::unexpected(Error(state->Database));
  }
  Statement check;
  if (sqlite3_prepare_v2(state->Database,
                         "SELECT key,stored_bytes,created_unix_s,last_used_unix_s "
                         "FROM cache_usage_v1 LIMIT 0",
                         -1,
                         &check.Handle,
                         nullptr) != SQLITE_OK) {
    return std::unexpected(Error(state->Database));
  }
  return std::unique_ptr<CacheUsageIndex>(new CacheUsageIndex(std::move(state)));
}

std::expected<void, std::string>
CacheUsageIndex::Record(std::string_view key, uint64_t storedBytes, bool published) {
  if (key.size() != 64 ||
      !std::ranges::all_of(
          key, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }) ||
      storedBytes > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    return std::unexpected("cache usage index requires a SHA-256 key and representable byte count");
  }
  const auto now = std::chrono::duration_cast<std::chrono::duration<int64_t>>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  const std::scoped_lock lock(State_->Mutex);
  if (State_->Pending.size() == kBatchEntries && !State_->Pending.contains(std::string(key))) {
    if (auto written = Write(State_->Database, State_->Pending); !written) { return written; }
    State_->Pending.clear();
  }
  auto &observation = State_->Pending[std::string(key)];
  observation.Bytes = storedBytes;
  observation.Used = std::max(observation.Used, now);
  if (published && !observation.Created) { observation.Created = now; }
  return {};
}

std::expected<void, std::string> CacheUsageIndex::Flush() {
  const std::scoped_lock lock(State_->Mutex);
  if (auto written = Write(State_->Database, State_->Pending); !written) { return written; }
  State_->Pending.clear();
  return {};
}

size_t CacheUsageIndex::Pending() const {
  const std::scoped_lock lock(State_->Mutex);
  return State_->Pending.size();
}

}
