#include "RuntimeFixture.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <dlfcn.h>
#include <expected>
#include <filesystem>
#include <optional>
#include <memory>
#include <semaphore>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <thread>

namespace {
using namespace AssetRuntimeFixture;

struct Control {
  std::string Directory;
  std::string Database;
  std::thread::id Frame = std::this_thread::get_id();
  std::shared_ptr<JobAdmission> Connections;
  std::atomic<unsigned> Opens{0};
  std::atomic<sqlite3 *> First{nullptr}, Second{nullptr};
  std::atomic<bool> BlockRead{false}, Closed{false}, ClosedOnFrame{false};
  std::atomic<int> CreditsAtClose{0};
  std::binary_semaphore Entered{0}, Release{0};
};

std::atomic<Control *> Hook{nullptr};

struct Observe {
  explicit Observe(Control &state) { Hook.store(&state); }

  ~Observe() { Hook.store(nullptr); }

  Observe(const Observe &) = delete;
  Observe &operator=(const Observe &) = delete;
};

int Trace(unsigned event, void * /*context*/, void *statement, void * /*detail*/) {
  auto *observed = Hook.load();
  if (observed == nullptr) { return 0; }
  auto &control = *observed;
  if (event == SQLITE_TRACE_STMT) {
    const char *sql = sqlite3_sql(static_cast<sqlite3_stmt *>(statement));
    if (sql != nullptr && std::string_view(sql).contains("FROM assets a WHERE") &&
        control.BlockRead.exchange(false)) {
      control.Entered.release();
      (void)control.Release.try_acquire_for(5s);
    }
  }
  return 0;
}
}

extern "C" int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs) {
  using Open = int (*)(const char *, sqlite3 **, int, const char *);
  static const auto open = reinterpret_cast<Open>(dlsym(RTLD_NEXT, "sqlite3_open_v2"));
  if (open == nullptr) { return SQLITE_CANTOPEN; }
  const int result = open(filename, ppDb, flags, zVfs);
  auto *control = Hook.load();
  if (result == SQLITE_OK && control != nullptr && filename != nullptr) {
    if (std::string_view(filename).starts_with(control->Directory)) { ++control->Opens; }
    if (filename == control->Database) {
      sqlite3 *empty = nullptr;
      if (!control->First.compare_exchange_strong(empty, *ppDb)) { control->Second.store(*ppDb); }
      (void)sqlite3_trace_v2(*ppDb, SQLITE_TRACE_STMT, Trace, nullptr);
    }
  }
  return result;
}

extern "C" int sqlite3_close(sqlite3 *database) {
  using Close = int (*)(sqlite3 *);
  static const auto close = reinterpret_cast<Close>(dlsym(RTLD_NEXT, "sqlite3_close"));
  if (close == nullptr) { return SQLITE_MISUSE; }
  auto *control = Hook.load();
  const bool observed = control != nullptr && database == control->Second.load();
  if (observed) {
    control->ClosedOnFrame.store(std::this_thread::get_id() == control->Frame);
    control->CreditsAtClose.store(control->Connections->Active());
  }
  const int result = close(database);
  if (observed) { control->Closed.store(true); }
  return result;
}

int main() {
  using namespace AssetRuntimeFixture;
  const Directory directory;
  const auto connections = std::make_shared<JobAdmission>(2);
  const auto path = (directory.Path / "current" / "assets.sqlite").string();
  Control control{
      .Directory = directory.Path.string(), .Database = path, .Connections = connections};
  Tasks workers(2);
  const Observe observe(control);
  const AssetCache::Runtime runtime{.Workers = &workers, .Connections = connections};
  auto first = AssetCache::Open(path, runtime);
  auto second = AssetCache::Open(path, runtime);
  const auto nextPath = directory.Path / "next" / "assets.sqlite";
  auto next = AssetCache::Open(nextPath.string(), runtime);
  CHECK(first && second && next, "runtime configuration accepts valid cache locations");
  if (!first || !second || !next) { return Report(); }
  CHECK(control.Opens.load() == 0 && connections->Active() == 0 &&
            !std::filesystem::exists(directory.Path),
        "configuration performs no SQLite open, creates no directory and admits no connection");
  CHECK(AwaitFind(workers, **first) && AwaitFind(workers, **second) && connections->Active() == 2,
        "the two first actual IO operations acquire independent bounded connections");
  const auto opens = control.Opens.load();
  control.BlockRead.store(true);
  std::binary_semaphore finished(0);
  std::expected<std::optional<AssetRecord>, AssetCacheError> reading =
      std::unexpected(AssetCacheError::Storage);
  const bool posted = workers.PostDetached([&] {
    reading = (*first)->Find(kMissing);
    finished.release();
  });
  CHECK(posted && control.Entered.try_acquire_for(5s),
        "the real SQLite statement holds the shared database mutex");
  const auto began = std::chrono::steady_clock::now();
  second->reset();
  CHECK(std::chrono::steady_clock::now() - began < 250ms && !control.Closed.load() &&
            connections->Active() == 2,
        "last-handle release does not wait for the real database mutex in the frame");
  const auto busy = (*next)->Find(kMissing);
  CHECK(!busy && busy.error() == AssetCacheError::Busy && control.Opens.load() == opens &&
            !std::filesystem::exists(nextPath.parent_path()),
        "closing handles retain admission and a full budget refuses IO before opening");
  control.Release.release();
  if (posted) { finished.acquire(); }
  CHECK(reading, "the original reader completes unchanged");
  const auto until = std::chrono::steady_clock::now() + 5s;
  while (connections->Active() != 1 && std::chrono::steady_clock::now() < until) {
    std::this_thread::yield();
  }
  CHECK(control.Closed.load() && !control.ClosedOnFrame.load() &&
            control.CreditsAtClose.load() == 2 && connections->Active() == 1,
        "the worker closes SQLite before returning the retired connection credit");
  CHECK(AwaitFind(workers, **next) && connections->Active() == 2,
        "the unchanged pending cache advances after physical close releases admission");
  first->reset();
  next->reset();
  while (connections->Active() != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::yield();
  }
  CHECK(connections->Active() == 0, "all actual runtime connections eventually retire");
  return Report();
}
