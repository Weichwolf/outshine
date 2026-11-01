#include "SqliteFileBudgetState.h"
#include "FileBudgetSystem.h"
#include "ReadTextFile.h"
#include "WriteFileAtomically.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace outshine {
thread_local std::vector<SqliteFileBudget::State *> SqliteFileBudget::State::Maintaining;
thread_local bool SqliteFileBudget::State::Denied = false;

namespace {
constexpr unsigned kMaximumBudgetShift = 60;
constexpr uint64_t kMiB = uint64_t{1024} * 1024;
std::atomic<uint64_t> Serial{0};

struct Descriptor {
  int Value = -1;

  ~Descriptor() {
    if (Value >= 0) { FileBudgetSystem::Close(Value); }
  }
};

std::expected<uint64_t, std::string> ReadLimit(const std::filesystem::path &root) {
  std::error_code error;
  const auto path = root / "budget.json";
  if (!std::filesystem::exists(path, error)) {
    if (error) { return std::unexpected(error.message()); }
    return SqliteFileBudget::DefaultBytes;
  }
  const auto text = ReadTextFile(path.string(), 4096);
  if (!text) { return std::unexpected(text.error()); }
  const auto value = nlohmann::json::parse(*text, nullptr, false);
  if (value.is_discarded() || !value.is_object() || value.size() != 2 ||
      !value.contains("version") || !value["version"].is_number_unsigned() ||
      value["version"].get<uint64_t>() != 1 || !value.contains("limit_bytes") ||
      !value["limit_bytes"].is_number_unsigned()) {
    return std::unexpected("invalid asset file budget settings");
  }
  const auto bytes = value["limit_bytes"].get<uint64_t>();
  if (bytes < 4 * kMiB || bytes > uint64_t{1} << kMaximumBudgetShift) {
    return std::unexpected("asset file budget must be between 4 MiB and 1 EiB");
  }
  return bytes;
}

std::expected<std::filesystem::path, std::string> RootPath(const std::filesystem::path &path) {
  if (path.empty() || path.native().contains('\0')) {
    return std::unexpected("asset budget requires a valid directory");
  }
  std::error_code error;
  auto root = std::filesystem::canonical(path, error);
  if (error || !std::filesystem::is_directory(root, error)) {
    return std::unexpected("asset budget requires an existing directory");
  }
  return root;
}

bool Add(uint64_t &total, uint64_t value) {
  if (value > std::numeric_limits<uint64_t>::max() - total) { return false; }
  total += value;
  return true;
}
}

SqliteFileBudget::State::~State() {
  if (Registered) { sqlite3_vfs_unregister(&Vfs); }
  if (Growth >= 0) { FileBudgetSystem::Close(Growth); }
  if (Activity >= 0) { FileBudgetSystem::Close(Activity); }
}

std::expected<SqliteFileBudget::Usage, std::string>
SqliteFileBudget::State::Inspect(const std::filesystem::path &target) const {
  Usage usage;
  std::error_code error;
  std::filesystem::recursive_directory_iterator files(Root, error);
  if (error) { return std::unexpected(error.message()); }
  for (const auto end = std::filesystem::recursive_directory_iterator(); files != end;
       files.increment(error)) {
    if (error) { return std::unexpected(error.message()); }
    if (files->is_symlink(error)) {
      return std::unexpected("asset budget files must not be symlinks");
    }
    if (files->is_directory(error)) { continue; }
    const auto info = FileBudgetSystem::Inspect(files->path());
    if (!info) { return std::unexpected(info.error()); }
    if (!*info) { continue; }
    const auto bytes = (**info).Bytes;
    const auto allocated = (**info).Allocated;
    const auto charged = std::max(bytes, allocated);
    if (!Add(usage.FileBytes, bytes) || !Add(usage.AllocatedBytes, allocated) ||
        !Add(usage.ChargedBytes, charged) ||
        (files->path().filename().string().ends_with("-wal") && !Add(usage.WalBytes, bytes))) {
      return std::unexpected("asset budget size overflow");
    }
    if (files->path() == target) { usage.TargetCharge = charged; }
  }
  if (error) { return std::unexpected(error.message()); }
  return usage;
}

std::expected<void, std::string> SqliteFileBudget::State::Initialize() {
  Activity = FileBudgetSystem::OpenLock(Root / "budget.active");
  if (Activity < 0 || !FileBudgetSystem::Lock(Activity, false, false)) {
    return std::unexpected("asset budget settings are in use");
  }
  Growth = FileBudgetSystem::OpenLock(Root / "budget.lock");
  if (Growth < 0) { return std::unexpected("cannot open asset growth lock"); }
  const auto limit = ReadLimit(Root);
  if (!limit) { return std::unexpected(limit.error()); }
  Bytes = *limit;
  MaintenanceBytes = std::max(kMiB, Bytes / 32);
  const auto block = FileBudgetSystem::BlockBytes(Root);
  if (!block) { return std::unexpected(block.error()); }
  BlockBytes = *block;
  const auto used = Inspect();
  if (!used) { return std::unexpected(used.error()); }
  if (used->ChargedBytes > Bytes) {
    RecoveryCeiling = used->ChargedBytes;
    if (!Add(RecoveryCeiling, MaintenanceBytes)) {
      return std::unexpected("asset recovery budget overflow");
    }
  }
  Parent = sqlite3_vfs_find(nullptr);
  if (Parent == nullptr || Parent->iVersion < 3) {
    return std::unexpected("asset budget needs SQLite VFS version 3");
  }
  Name = "outshine-file-budget-" + std::to_string(Serial.fetch_add(1));
  Vfs = *Parent;
  Vfs.iVersion = 3;
  Vfs.zName = Name.c_str();
  Vfs.pNext = nullptr;
  Vfs.pAppData = this;
  Vfs.xOpen = OpenFile;
  return Register();
}

std::expected<std::shared_ptr<SqliteFileBudget>, std::string>
SqliteFileBudget::Open(const std::filesystem::path &directory) {
  const auto root = RootPath(directory);
  if (!root) { return std::unexpected(root.error()); }
  static std::mutex registryMutex;
  static std::unordered_map<std::string, std::weak_ptr<SqliteFileBudget>> registry;
  const std::scoped_lock lock(registryMutex);
  std::erase_if(registry, [](const auto &entry) { return entry.second.expired(); });
  auto &slot = registry[root->string()];
  if (auto live = slot.lock()) { return live; }
  auto state = std::make_shared<State>();
  state->Root = *root;
  state->Self = state;
  if (const auto initialized = state->Initialize(); !initialized) {
    return std::unexpected(initialized.error());
  }
  auto owner = std::shared_ptr<SqliteFileBudget>(new SqliteFileBudget(std::move(state)));
  slot = owner;
  return owner;
}

std::expected<void, std::string> SqliteFileBudget::Configure(const std::filesystem::path &directory,
                                                             uint64_t bytes) {
  if (bytes < 4 * kMiB || bytes > uint64_t{1} << kMaximumBudgetShift) {
    return std::unexpected("asset file budget must be between 4 MiB and 1 EiB");
  }
  const auto root = RootPath(directory);
  if (!root) { return std::unexpected(root.error()); }
  const Descriptor active{FileBudgetSystem::OpenLock(*root / "budget.active")};
  if (active.Value < 0 || !FileBudgetSystem::Lock(active.Value, true, false)) {
    return std::unexpected("close asset cache users before changing its budget");
  }
  const auto text = nlohmann::json{{"version", 1}, {"limit_bytes", bytes}}.dump() + "\n";
  return WriteFileAtomically((*root / "budget.json").string(),
                             std::as_bytes(std::span(text.data(), text.size())));
}

SqliteFileBudget::State::Mutation::Mutation(State &state) : Owner(state), Lock(state.Mutex) {
  if (FileBudgetSystem::Lock(Owner.Growth, true, true)) { Result = SQLITE_OK; }
}

SqliteFileBudget::State::Mutation::~Mutation() {
  if (Result == SQLITE_OK) { FileBudgetSystem::Unlock(Owner.Growth); }
}

int SqliteFileBudget::State::Mutation::Admit(const std::filesystem::path &path,
                                             uint64_t end,
                                             bool checkpointReserve) {
  if (Result != SQLITE_OK) { return Result; }
  const auto used = Owner.Inspect(path);
  if (!used) { return SQLITE_IOERR_FSTAT; }
  if (end > std::numeric_limits<uint64_t>::max() - (Owner.BlockBytes - 1)) {
    Denied = true;
    return SQLITE_FULL;
  }
  const auto rounded = ((end + Owner.BlockBytes - 1) / Owner.BlockBytes) * Owner.BlockBytes;
  uint64_t charge = used->ChargedBytes - used->TargetCharge;
  if (!Add(charge, std::max(used->TargetCharge, rounded))) {
    Denied = true;
    return SQLITE_FULL;
  }
  uint64_t recovered = used->ChargedBytes;
  if (Add(recovered, used->WalBytes) && Add(recovered, Owner.MaintenanceBytes) &&
      recovered <= Owner.Bytes) {
    Owner.RecoveryCeiling = 0;
  }
  const auto hard = std::max(Owner.Bytes, Owner.RecoveryCeiling);
  uint64_t reserved = charge;
  const bool regular =
      checkpointReserve && std::ranges::find(Maintaining, &Owner) == Maintaining.end();
  uint64_t wal = used->WalBytes;
  if (path.filename().string().ends_with("-wal")) {
    std::error_code error;
    const auto old = std::filesystem::file_size(path, error);
    if (error && error != std::errc::no_such_file_or_directory) { return SQLITE_IOERR_FSTAT; }
    const auto previous = error ? uint64_t{0} : old;
    if (end > previous && !Add(wal, end - previous)) {
      Denied = true;
      return SQLITE_FULL;
    }
  }
  if (charge > hard || (regular && (!Add(reserved, wal) || !Add(reserved, Owner.MaintenanceBytes) ||
                                    reserved > Owner.Bytes))) {
    Denied = true;
    return SQLITE_FULL;
  }
  return SQLITE_OK;
}

SqliteFileBudget::SqliteFileBudget(std::shared_ptr<State> state) : State_(std::move(state)) {}

SqliteFileBudget::~SqliteFileBudget() = default;

const char *SqliteFileBudget::VfsName() const noexcept {
  return State_->Name.c_str();
}

uint64_t SqliteFileBudget::Limit() const noexcept {
  return State_->Bytes;
}

uint64_t SqliteFileBudget::Reserve() const noexcept {
  return State_->MaintenanceBytes;
}

const std::filesystem::path &SqliteFileBudget::Directory() const noexcept {
  return State_->Root;
}

std::expected<SqliteFileBudget::Usage, std::string> SqliteFileBudget::Inspect() const {
  const std::scoped_lock lock(State_->Mutex);
  return State_->Inspect();
}

bool SqliteFileBudget::TakeDenial() noexcept {
  return std::exchange(State::Denied, false);
}

bool SqliteFileBudget::WasDenied() noexcept {
  return State::Denied;
}

void SqliteFileBudget::ClearDenial() noexcept {
  State::Denied = false;
}

SqliteFileBudget::Maintenance::Maintenance(const SqliteFileBudget &budget) : State_(budget.State_) {
  State::Maintaining.push_back(State_.get());
}

SqliteFileBudget::Maintenance::~Maintenance() {
  const auto at = std::ranges::find(State::Maintaining, State_.get());
  if (at != State::Maintaining.end()) { State::Maintaining.erase(at); }
}
}
