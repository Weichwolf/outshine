#include "AssetCache.h"
#include "Check.h"
#include <chrono>
#include <filesystem>
#include <sqlite3.h>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-ready-cache-readers-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  const auto path = (directory / "assets.sqlite").string();
  {
    const auto initialized = AssetCache::Open(path);
    CHECK(initialized, "the native cache schema and usage table are initialized");
  }
  sqlite3 *writer = nullptr;
  CHECK(sqlite3_open(path.c_str(), &writer) == SQLITE_OK, "independent writer opens the cache");
  if (writer == nullptr) { return Report(); }
  const int locked = sqlite3_exec(writer, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
  CHECK(locked == SQLITE_OK, "another preparation job holds the SQLite writer lock");
  {
    auto reader = AssetCache::Open(path);
    CHECK(reader, "a ready-cache reader opens without requesting a schema migration write lock");
    if (reader) {
      const auto found = (*reader)->FindRequest(std::string(64, 'a'));
      CHECK(found && !*found, "the concurrent reader sees the unchanged committed empty cache");
    }
    CHECK(sqlite3_get_autocommit(writer) == 0, "the independent writer still owns its transaction");
    CHECK(sqlite3_exec(writer, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer can finish its unchanged transaction");
  }
  CHECK(sqlite3_close(writer) == SQLITE_OK, "the independent writer releases its connection");
  std::filesystem::remove_all(directory);
  return Report();
}
