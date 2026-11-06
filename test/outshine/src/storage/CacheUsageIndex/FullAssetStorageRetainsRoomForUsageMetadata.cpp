#include "CacheUsageIndex.h"
#include "SqliteFileBudget.h"
#include "Tasks.h"
#include "Check.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <system_error>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr uint64_t kLimit = uint64_t{12} * 1024 * 1024;
constexpr uint64_t kChunk = uint64_t{64} * 1024;

struct Directory {
  const std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-full-cache-recency-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  Directory() {
    std::error_code error;
    std::filesystem::create_directories(Path, error);
    CHECK(!error, "the isolated game cache directory is created");
  }

  ~Directory() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};

bool Fill(sqlite3 *database, const std::shared_ptr<SqliteFileBudget> &budget) {
  if (sqlite3_exec(database,
                   "PRAGMA journal_mode=WAL;PRAGMA synchronous=NORMAL;"
                   "CREATE TABLE payload(bytes BLOB)",
                   nullptr,
                   nullptr,
                   nullptr) != SQLITE_OK) {
    return false;
  }
  int result = SQLITE_OK;
  for (uint64_t step = 0; result == SQLITE_OK && step <= kLimit / kChunk; ++step) {
    result = sqlite3_exec(
        database, "INSERT INTO payload VALUES(zeroblob(65536))", nullptr, nullptr, nullptr);
    if (result == SQLITE_OK &&
        sqlite3_wal_checkpoint_v2(
            database, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr) != SQLITE_OK) {
      return false;
    }
  }
  CHECK(result == SQLITE_FULL, "saved content reaches its regular storage quota");
  if (result != SQLITE_FULL) { return false; }
  if (sqlite3_wal_checkpoint_v2(database, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr) !=
      SQLITE_OK) {
    return false;
  }
  {
    const SqliteFileBudget::Maintenance maintenance(*budget);
    const std::string sql =
        std::format("INSERT INTO payload VALUES(zeroblob({}))", budget->Reserve() / 4);
    CHECK(sqlite3_exec(database, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
          "existing checkpoint work occupies part of the reserved headroom");
  }
  const auto used = budget->Inspect();
  CHECK(used && used->ChargedBytes + budget->Reserve() > budget->Limit() &&
            used->ChargedBytes < budget->Limit(),
        "regular writes are full while physical maintenance capacity remains");
  return used.has_value();
}

int64_t Count(const std::string &path) {
  sqlite3 *raw = nullptr;
  const int opened = sqlite3_open(path.c_str(), &raw);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, &sqlite3_close);
  if (opened != SQLITE_OK) { return -1; }
  sqlite3_stmt *prepared = nullptr;
  const int ready = sqlite3_prepare_v2(
      database.get(), "SELECT count(*) FROM cache_usage_v1", -1, &prepared, nullptr);
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(prepared,
                                                                             &sqlite3_finalize);
  return ready == SQLITE_OK && sqlite3_step(statement.get()) == SQLITE_ROW
             ? sqlite3_column_int64(statement.get(), 0)
             : -1;
}
}

int main() {
  const Directory directory;
  CHECK(SqliteFileBudget::Configure(directory.Path, kLimit), "a small game cache is configured");
  const auto budget = SqliteFileBudget::Open(directory.Path);
  CHECK(budget, "the physical asset storage budget opens");
  if (!budget) { return Report(); }
  const auto path = (directory.Path / "usage.sqlite").string();
  Tasks workers(1);
  auto index = CacheUsageIndex::Open(path, *budget, &workers);
  CHECK(index, "runtime usage observations share the actual asset quota");
  if (!index) { return Report(); }
  sqlite3 *raw = nullptr;
  const int opened = sqlite3_open_v2((directory.Path / "assets.sqlite").c_str(),
                                     &raw,
                                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                     (*budget)->VfsName());
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> payload(raw, &sqlite3_close);
  CHECK(opened == SQLITE_OK, "saved game content uses the same quota-enforcing VFS");
  if (opened != SQLITE_OK || !Fill(payload.get(), *budget)) { return Report(); }
  for (unsigned i = 0; i < 256; ++i) {
    CHECK((*index)->Record(std::format("{:064x}", i), 48),
          "revisiting saved assets records their recency without allocating content storage");
  }
  const auto drained = workers.Post([] {});
  workers.Wait(drained);
  CHECK(Count(path) == 256, "the background recency batch uses reserved metadata capacity");
  CHECK((*index)->Flush(), "explicit maintenance also retains the captured recency");
  const auto used = (*budget)->Inspect();
  CHECK(used && used->ChargedBytes <= (*budget)->Limit(),
        "metadata never bypasses the physical cache limit");
  return Report();
}
