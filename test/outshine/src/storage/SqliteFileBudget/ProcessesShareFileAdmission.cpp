#include "SqliteFileBudget.h"
#include "Check.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <expected>
#include <cstddef>
#include <string_view>
#include <fstream>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
#include <csignal>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#ifndef _WIN32
namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr uint64_t kLimit = uint64_t{12} * size_t{1024} * 1024;

int Worker(const std::filesystem::path &root, const char *name) {
  const auto budget = SqliteFileBudget::Open(root);
  if (!budget) { return 10; }
  sqlite3 *raw = nullptr;
  const int opened =
      sqlite3_open_v2((root / name).c_str(),
                      &raw,
                      SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX,
                      (*budget)->VfsName());
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
  if (opened != SQLITE_OK) { return 11; }
  if (sqlite3_exec(raw,
                   "PRAGMA journal_mode=WAL;PRAGMA wal_autocheckpoint=0;"
                   "CREATE TABLE products(id INTEGER PRIMARY KEY,bytes BLOB)",
                   nullptr,
                   nullptr,
                   nullptr) != SQLITE_OK) {
    return 12;
  }
  int accepted = 0;
  bool denied = false;
  for (int id = 0; id < 128; ++id) {
    const auto sql = "INSERT INTO products VALUES(" + std::to_string(id) + ",zeroblob(131072))";
    bool published = false;
    for (int retry = 0; retry < 1024; ++retry) {
      const int code = sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr);
      if (code == SQLITE_OK) {
        published = true;
        ++accepted;
        break;
      }
      (void)sqlite3_exec(raw, "ROLLBACK", nullptr, nullptr, nullptr);
      if ((static_cast<unsigned>(code) & 0xffU) == SQLITE_FULL) {
        denied = true;
        break;
      }
      if ((static_cast<unsigned>(code) & 0xffU) != SQLITE_BUSY &&
          (static_cast<unsigned>(code) & 0xffU) != SQLITE_LOCKED) {
        return 13;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!published) { break; }
    const auto usage = (*budget)->Inspect();
    if (!usage || usage->ChargedBytes > kLimit) { return 14; }
  }
  if (accepted == 0 || !denied) { return 15; }
  return 0;
}

bool Wait(pid_t pid) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  int status = 0;
  while (std::chrono::steady_clock::now() < until) {
    const auto result = waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      CHECK(WIFEXITED(status), "writer terminates normally");
      if (!WIFEXITED(status)) { return false; }
      const auto message = "writer exit code " + std::to_string(WEXITSTATUS(status));
      CHECK(WEXITSTATUS(status) == 0, message.c_str());
      return WEXITSTATUS(status) == 0;
    }
    if (result < 0) { return false; }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  (void)kill(pid, SIGKILL);
  (void)waitpid(pid, &status, 0);
  return false;
}

std::string Executable() {
#ifdef __APPLE__
  constexpr uint32_t kPathBytes = 4096;
  uint32_t bytes = kPathBytes;
  std::vector<char> path(bytes);
  if (_NSGetExecutablePath(path.data(), &bytes) != 0) {
    path.resize(bytes);
    if (_NSGetExecutablePath(path.data(), &bytes) != 0) { return {}; }
  }
  return path.data();
#else
  std::error_code error;
  const auto path = std::filesystem::read_symlink("/proc/self/exe", error);
  return error ? std::string{} : path.string();
#endif
}

void Processes() {
  const auto executable = Executable();
  CHECK(!executable.empty(), "the operating system identifies this test binary");
  if (executable.empty()) { return; }
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-budget-processes-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  CHECK(SqliteFileBudget::Configure(directory, kLimit),
        "both processes read one persistent file budget");
  std::vector<pid_t> children;
  for (const char *name : {"assets.sqlite", "prototypes.sqlite"}) {
    const pid_t pid = fork();
    if (pid == 0) {
      execl(executable.c_str(), executable.c_str(), "--worker", directory.c_str(), name, nullptr);
      _exit(127);
    }
    CHECK(pid > 0, "a fresh process starts with independent SQLite and budget state");
    if (pid > 0) { children.push_back(pid); }
  }
  for (const auto pid : children) {
    CHECK(Wait(pid), "each independent writer reaches bounded backpressure");
  }
  const auto budget = SqliteFileBudget::Open(directory);
  const auto usage = budget ? (*budget)->Inspect() : std::unexpected(std::string("missing root"));
  CHECK(usage && usage->ChargedBytes <= kLimit,
        "two processes preserve the combined physical file charge");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
}
}

int main(int argc, char *const *argv) {
  if (argc == 4 && std::string_view(argv[1]) == "--worker") { return Worker(argv[2], argv[3]); }
  Processes();
  return outshine::Test::Report();
}

#else
int main() {
  outshine::Test::Skip("POSIX process admission probe; Windows device validation remains separate");
  return outshine::Test::Report();
}
#endif
