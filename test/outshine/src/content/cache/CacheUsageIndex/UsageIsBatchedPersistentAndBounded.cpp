#include "src/content/cache/CacheUsageIndex.h"
#include "Check.h"
#include <chrono>
#include <filesystem>
#include <format>
#include <limits>
#include <sqlite3.h>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Test;

int64_t Scalar(sqlite3 *database, const char *sql) {
  sqlite3_stmt *statement = nullptr;
  const int prepared = sqlite3_prepare_v2(database, sql, -1, &statement, nullptr);
  CHECK(prepared == SQLITE_OK, "independent metadata query prepares");
  if (prepared != SQLITE_OK) { return -1; }
  const int step = sqlite3_step(statement);
  CHECK(step == SQLITE_ROW, "independent metadata query returns a row");
  const int64_t result = step == SQLITE_ROW ? sqlite3_column_int64(statement, 0) : -1;
  CHECK(sqlite3_finalize(statement) == SQLITE_OK, "independent metadata query closes");
  return result;
}

void Observations(const std::string &path) {
  auto index = CacheUsageIndex::Open(path);
  CHECK(index, "metadata store opens");
  if (!index) { return; }
  sqlite3 *database = nullptr;
  CHECK(sqlite3_open(path.c_str(), &database) == SQLITE_OK, "independent reader opens");
  const std::string key(64, 'a');
  CHECK((*index)->Record(key, 40), "legacy observation records use without inventing creation");
  CHECK((*index)->Record(key, 48), "repeated observation coalesces");
  CHECK((*index)->Pending() == 1, "repeated source or package keys share one pending observation");
  CHECK(Scalar(database, "SELECT count(*) FROM cache_usage_v1") == 0,
        "individual accesses do not write the database");
  CHECK((*index)->Flush(), "explicit batch commits");
  CHECK(Scalar(database, "SELECT stored_bytes FROM cache_usage_v1") == 48,
        "latest stored byte count persists");
  CHECK(Scalar(database, "SELECT created_unix_s IS NULL FROM cache_usage_v1") == 1,
        "unknown legacy creation stays explicitly unknown");
  CHECK(sqlite3_exec(database,
                     "CREATE TRIGGER reject_redundant_usage BEFORE UPDATE ON cache_usage_v1 "
                     "BEGIN SELECT RAISE(ABORT,'redundant metadata write');END",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "independent write rejection installs");
  CHECK((*index)->Record(key, 48) && (*index)->Flush(),
        "repeated hits within the minute do not write an unchanged usage row");
  CHECK(sqlite3_exec(database, "DROP TRIGGER reject_redundant_usage", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "independent write rejection is removed");
  CHECK(sqlite3_exec(database,
                     "UPDATE cache_usage_v1 SET last_used_unix_s=4000000000",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "future-clock counterexample installs");
  CHECK((*index)->Record(key, 48, true) && (*index)->Flush(), "known publication adds creation");
  CHECK(Scalar(database, "SELECT created_unix_s>0 FROM cache_usage_v1") == 1,
        "known publication time persists");
  CHECK(Scalar(database, "SELECT last_used_unix_s FROM cache_usage_v1") == 4000000000,
        "clock rollback never moves recorded use backwards");
  CHECK(!(*index)->Record("invalid", 10), "malformed identity is rejected");
  CHECK(!(*index)->Record(key, std::numeric_limits<uint64_t>::max()),
        "unrepresentable byte count is rejected");
  CHECK((*index)->Record(std::string(64, 'b'), 99, true), "final partial batch is pending");
  index->reset();
  CHECK(Scalar(database, "SELECT count(*) FROM cache_usage_v1") == 2,
        "normal owner shutdown persists the partial batch");
  CHECK(sqlite3_close(database) == SQLITE_OK, "independent reader closes");
}

void BusyAndCapacity(const std::string &path) {
  auto index = CacheUsageIndex::Open(path);
  CHECK(index, "bounded metadata store opens");
  if (!index) { return; }
  sqlite3 *database = nullptr;
  CHECK(sqlite3_open(path.c_str(), &database) == SQLITE_OK, "write blocker opens");
  CHECK(sqlite3_exec(database, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "independent writer holds the database");
  for (size_t i = 0; i < 256; ++i) {
    CHECK((*index)->Record(std::format("{:064x}", i), i), "bounded pending observation records");
  }
  CHECK(!(*index)->Record(std::string(64, 'f'), 1), "busy batch flush reports an explicit failure");
  CHECK((*index)->Pending() == 256, "failed writes do not permit unbounded metadata growth");
  CHECK(sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "write blocker releases");
  CHECK((*index)->Flush(), "the same observations survive retry");
  CHECK(Scalar(database, "SELECT count(*) FROM cache_usage_v1") == 256,
        "retry persists every retained observation");
  CHECK((*index)->Pending() == 0, "successful flush releases pending storage");
  CHECK(sqlite3_close(database) == SQLITE_OK, "write blocker closes");
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-cache-usage-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  CHECK(!CacheUsageIndex::Open(""), "empty index path is rejected");
  Observations((directory / "usage.sqlite").string());
  BusyAndCapacity((directory / "busy.sqlite").string());
  std::filesystem::remove_all(directory);
  return Report();
}
