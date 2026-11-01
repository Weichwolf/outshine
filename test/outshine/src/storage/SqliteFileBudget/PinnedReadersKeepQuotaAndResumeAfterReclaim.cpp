#include "SqliteFileBudget.h"
#include "Check.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <expected>
#include <cstddef>
#include <string_view>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr uint64_t kLimit = uint64_t{12} * size_t{1024} * 1024;

struct Directory {
  std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-budget-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  Directory() { std::filesystem::create_directories(Path); }

  ~Directory() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};

using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

Database Open(const SqliteFileBudget &budget) {
  sqlite3 *db = nullptr;
  const int opened =
      sqlite3_open_v2((budget.Directory() / "assets.sqlite").c_str(),
                      &db,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
                      budget.VfsName());
  CHECK(opened == SQLITE_OK, "the native budget VFS opens a real SQLite database");
  return {db, sqlite3_close};
}

int Exec(sqlite3 *db, const char *sql) {
  return sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
}

int Scalar(sqlite3 *db, const char *sql) {
  sqlite3_stmt *raw = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) { return -1; }
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
  return sqlite3_step(raw) == SQLITE_ROW ? sqlite3_column_int(raw, 0) : -1;
}

int Publish(sqlite3 *db, int id) {
  const auto sql = "INSERT INTO products VALUES(" + std::to_string(id) + ",zeroblob(131072))";
  const int result = Exec(db, sql.c_str());
  if (result != SQLITE_OK) { (void)Exec(db, "ROLLBACK"); }
  return result;
}

bool Integrity(sqlite3 *db) {
  sqlite3_stmt *raw = nullptr;
  if (sqlite3_prepare_v2(db, "PRAGMA integrity_check", -1, &raw, nullptr) != SQLITE_OK) {
    return false;
  }
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
  if (sqlite3_step(raw) != SQLITE_ROW) { return false; }
  const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(raw, 0));
  return text != nullptr && std::string_view(text) == "ok";
}

void Pinned() {
  const Directory directory;
  CHECK(SqliteFileBudget::Configure(directory.Path, kLimit),
        "a file budget persists before opening");
  const auto budget = SqliteFileBudget::Open(directory.Path);
  CHECK(budget && (*budget)->Limit() == kLimit, "the native root reads the persisted limit");
  if (!budget) { return; }
  CHECK(!SqliteFileBudget::Configure(directory.Path, kLimit * 2),
        "live clients prevent a contradictory concurrent budget change");
  const auto writer = Open(**budget);
  if (!writer) { return; }
  CHECK(Exec(writer.get(),
             "PRAGMA auto_vacuum=INCREMENTAL;PRAGMA journal_mode=WAL;"
             "PRAGMA synchronous=NORMAL;PRAGMA wal_autocheckpoint=0;"
             "CREATE TABLE products(id INTEGER PRIMARY KEY,bytes BLOB)") == SQLITE_OK,
        "the actual database uses WAL and incremental compaction");
  for (int id = 0; id < 8; ++id) {
    CHECK(Publish(writer.get(), id) == SQLITE_OK, "seed product persists");
  }
  CHECK(Scalar(writer.get(), "PRAGMA wal_checkpoint(TRUNCATE)") == 0, "initial checkpoint fits");
  auto reader = Open(**budget);
  CHECK(Exec(reader.get(), "BEGIN") == SQLITE_OK &&
            Scalar(reader.get(), "SELECT count(*) FROM products") == 8,
        "a real SQLite reader pins the initial products");
  int accepted = 8;
  int stopped = SQLITE_OK;
  for (int id = 8; id < 128; ++id) {
    stopped = Publish(writer.get(), id);
    if (stopped != SQLITE_OK) { break; }
    ++accepted;
    const auto usage = (*budget)->Inspect();
    CHECK(usage && usage->ChargedBytes <= kLimit,
          "each publication stays within the shared file budget");
  }
  CHECK((static_cast<unsigned>(stopped) & 0xffU) == SQLITE_FULL && SqliteFileBudget::TakeDenial(),
        "budget admission identifies its own SQLITE_FULL before unbounded WAL growth");
  CHECK(Scalar(writer.get(), "SELECT count(*) FROM products") == accepted &&
            Scalar(reader.get(), "SELECT count(*) FROM products") == 8,
        "failed publication preserves both committed products and the pinned snapshot");
  {
    const SqliteFileBudget::Maintenance maintenance(**budget);
    CHECK(Scalar(writer.get(), "PRAGMA wal_checkpoint(TRUNCATE)") == 1,
          "maintenance cannot revoke the pinned reader");
  }
  reader.reset();
  {
    const SqliteFileBudget::Maintenance maintenance(**budget);
    CHECK(Scalar(writer.get(), "PRAGMA wal_checkpoint(TRUNCATE)") == 0,
          "reserved file space allows checkpointing after the reader releases");
    CHECK(Exec(writer.get(), "DELETE FROM products WHERE id % 2 = 1") == SQLITE_OK,
          "maintenance removes only selected products");
    for (int step = 0; step < 4096 && Scalar(writer.get(), "PRAGMA freelist_count") > 0; ++step) {
      CHECK(Exec(writer.get(), "PRAGMA incremental_vacuum(32)") == SQLITE_OK &&
                Scalar(writer.get(), "PRAGMA wal_checkpoint(TRUNCATE)") == 0,
            "bounded reclamation succeeds");
    }
  }
  CHECK(Publish(writer.get(), 1000) == SQLITE_OK, "publication resumes after reclamation");
  CHECK(Integrity(writer.get()), "SQLite reports an intact database after pressure and compaction");
  const auto usage = (*budget)->Inspect();
  CHECK(usage && usage->ChargedBytes <= kLimit,
        "reclamation and resumed writes preserve the file limit");
}
}

int main() {
  Pinned();
  return outshine::Test::Report();
}
