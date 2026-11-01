#ifndef OUTSHINE_STORAGE_SQLITEFILEBUDGETSTATE_H
#define OUTSHINE_STORAGE_SQLITEFILEBUDGETSTATE_H
#include "SqliteFileBudget.h"
#include <filesystem>
#include <memory>
#include <mutex>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace outshine {
struct SqliteFileBudget::State {
  struct Mutation {
    explicit Mutation(State &state);
    ~Mutation();
    Mutation(const Mutation &) = delete;
    Mutation &operator=(const Mutation &) = delete;
    [[nodiscard]] int
    Admit(const std::filesystem::path &path, uint64_t end, bool checkpointReserve);
    State &Owner;
    std::unique_lock<std::mutex> Lock;
    int Result = SQLITE_BUSY;
  };

  std::filesystem::path Root;
  uint64_t Bytes = DefaultBytes;
  uint64_t MaintenanceBytes = 0;
  uint64_t RecoveryCeiling = 0;
  uint64_t BlockBytes = 4096;
  int Activity = -1;
  int Growth = -1;
  sqlite3_vfs *Parent = nullptr;
  sqlite3_vfs Vfs{};
  std::string Name;
  std::mutex Mutex;
  std::weak_ptr<State> Self;
  bool Registered = false;
  ~State();
  [[nodiscard]] std::expected<Usage, std::string>
  Inspect(const std::filesystem::path &target = {}) const;
  [[nodiscard]] std::expected<void, std::string> Initialize();
  [[nodiscard]] std::expected<void, std::string> Register();
  [[nodiscard]] static State &From(sqlite3_vfs *vfs);
  static int
  OpenFile(sqlite3_vfs *vfs, sqlite3_filename name, sqlite3_file *into, int flags, int *outFlags);
  static thread_local std::vector<State *> Maintaining;
  static thread_local bool Denied;
};
}
#endif
