#include "AssetCache.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <dlfcn.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <spawn.h>
#include <span>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#ifdef __APPLE__
#include <crt_externs.h>
#endif

namespace {
using namespace outshine;
using namespace outshine::Test;
const std::string kProduct(64, 'a');
const std::string kRequest(64, 'b');
constexpr std::array<uint8_t, 4> kPayload{17, 23, 31, 47};
using Connection = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

Connection Connect(const std::string &path) {
  sqlite3 *database = nullptr;
  if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
    if (database != nullptr) { (void)sqlite3_close(database); }
    database = nullptr;
  }
  return {database, sqlite3_close};
}

bool Execute(sqlite3 *database, const char *sql) {
  return sqlite3_exec(database, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

std::optional<int> Scalar(sqlite3 *database, const char *sql) {
  sqlite3_stmt *statement = nullptr;
  if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) { return {}; }
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> query(statement,
                                                                         sqlite3_finalize);
  if (sqlite3_step(query.get()) != SQLITE_ROW) { return {}; }
  return sqlite3_column_int(query.get(), 0);
}

int Migrate(const std::string &path, std::string_view version) {
  {
    auto cache = AssetCache::Open(path);
    CHECK(cache, "the independent process opens and migrates the original database");
    if (!cache) { return Report(); }
    const AssetRecord record{.Key = kProduct,
                             .Kind = "native",
                             .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                             .Package = {},
                             .ByteCount = kPayload.size(),
                             .Parent = {},
                             .RequestKey = kRequest};
    CHECK((*cache)->Publish(std::span(&record, 1), kPayload),
          "the independent migration publishes a real product and request alias");
  }
  if (version == "5") {
    const auto database = Connect(path);
    CHECK(database && Execute(database.get(), "PRAGMA user_version=5"),
          "the independent process can leave a newer schema version");
  }
  return Report();
}

bool Spawn(const std::string &executable, const std::string &path, int version) {
  std::array<std::string, 4> values{executable, "--migrate", path, std::to_string(version)};
  std::array<char *, 5> arguments{
      values[0].data(), values[1].data(), values[2].data(), values[3].data(), nullptr};
#ifdef __APPLE__
  char *const *const environment = *_NSGetEnviron();
#else
  char *const *const environment = environ;
#endif
  pid_t child = 0;
  if (posix_spawn(&child, executable.c_str(), nullptr, nullptr, arguments.data(), environment) !=
      0) {
    return false;
  }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  int status = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto waited = waitpid(child, &status, WNOHANG);
    if (waited == child) { return WIFEXITED(status) && WEXITSTATUS(status) == 0; }
    if (waited < 0 && errno != EINTR) { return false; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  (void)kill(child, SIGKILL);
  (void)waitpid(child, &status, 0);
  return false;
}

struct VersionGate {
  std::string Executable, Path;
  int NextVersion = 4;
  bool Observed = false, Migrated = false;
};

VersionGate *activeGate = nullptr;

int ObserveVersion(unsigned event, VersionGate &gate, sqlite3_stmt *statement) {
  const char *sql = sqlite3_sql(statement);
  if (event == SQLITE_TRACE_ROW && !gate.Observed && sql != nullptr &&
      std::string_view(sql).starts_with("PRAGMA user_version") &&
      sqlite3_column_int(statement, 0) == 0) {
    gate.Observed = true;
    gate.Migrated = Spawn(gate.Executable, gate.Path, gate.NextVersion);
  }
  return 0;
}

int ObserveRow(unsigned event, void *context, void *prepared, void * /*unused*/) {
  return ObserveVersion(
      event, *static_cast<VersionGate *>(context), static_cast<sqlite3_stmt *>(prepared));
}

void ConcurrentMigration(const std::string &executable, const std::string &path, int version) {
  {
    const auto database = Connect(path);
    CHECK(database && Execute(database.get(), "PRAGMA journal_mode=WAL"),
          "the source database is WAL with an unchanged uninitialized schema");
    if (!database) { return; }
  }
  VersionGate gate{.Executable = executable, .Path = path, .NextVersion = version};
  activeGate = &gate;
  auto opened = AssetCache::Open(path);
  activeGate = nullptr;
  CHECK(gate.Observed && gate.Migrated,
        "a fresh independent process commits after the first version observation");
  if (version == 4) {
    CHECK(opened, "a completed concurrent migration does not replay stale schema changes");
    if (opened) {
      const auto found = (*opened)->LoadRequest(kRequest, 1024);
      CHECK(found && *found && (**found).Bytes().size() == kPayload.size() &&
                std::ranges::equal((**found).Bytes(), kPayload),
            "the first process reads the exact product published by the concurrent migrator");
      const auto writer = Connect(path);
      CHECK(writer && Execute(writer.get(), "BEGIN IMMEDIATE"),
            "an independent writer can hold the current schema");
      const auto warm = AssetCache::Open(path);
      CHECK(warm, "a known current schema opens without acquiring a migration writer lock");
      CHECK(writer && Execute(writer.get(), "ROLLBACK"), "the independent writer releases");
    }
  } else {
    CHECK(!opened && opened.error() == AssetCacheError::UnsupportedVersion,
          "a newer concurrent schema is rejected before replaying old DDL");
  }
  const auto database = Connect(path);
  CHECK(database && Scalar(database.get(), "PRAGMA user_version") == version &&
            Scalar(database.get(), "SELECT count(*) FROM assets") == 1,
        "opening preserves the concurrent schema version and its committed product");
}
}

extern "C" int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs) {
  using Open = int (*)(const char *, sqlite3 **, int, const char *);
  static const auto open = reinterpret_cast<Open>(dlsym(RTLD_NEXT, "sqlite3_open_v2"));
  if (open == nullptr) { return SQLITE_CANTOPEN; }
  const int status = open(filename, ppDb, flags, zVfs);
  if (status == SQLITE_OK && activeGate != nullptr) {
    (void)sqlite3_trace_v2(*ppDb, SQLITE_TRACE_ROW, ObserveRow, activeGate);
  }
  return status;
}

int main(int argc, char *const *argv) {
  if (argc == 4 && std::string_view(argv[1]) == "--migrate") { return Migrate(argv[2], argv[3]); }
  const auto directory = std::filesystem::temp_directory_path() /
                         ("outshine-cache-open-race-" + std::to_string(getpid()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "concurrent schema fixtures use a private temporary directory");
  if (!error) {
    ConcurrentMigration(argv[0], (directory / "current.sqlite").string(), 4);
    ConcurrentMigration(argv[0], (directory / "future.sqlite").string(), 5);
  }
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "both processes release their private fixture databases");
  return Report();
}
