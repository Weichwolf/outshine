#include "SqliteFileBudgetState.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <expected>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <system_error>

namespace outshine {
namespace {
using State = SqliteFileBudget::State;

struct alignas(std::max_align_t) File : sqlite3_file {
  std::shared_ptr<State> Owner;
  sqlite3_file *Native = nullptr;
  sqlite3_io_methods Relay{};
  std::filesystem::path Path;
  sqlite3_filename OwnedName = nullptr;
  bool Temporary = false;
  bool Wal = false;
};

File &Of(sqlite3_file *file) {
  return *static_cast<File *>(file);
}

int Close(sqlite3_file *file) {
  auto &f = Of(file);
  const auto owner = f.Owner;
  const int result = f.Native->pMethods->xClose(f.Native);
  if (f.Temporary) {
    std::error_code error;
    std::filesystem::remove(f.Path, error);
  }
  if (f.OwnedName != nullptr) { sqlite3_free_filename(f.OwnedName); }
  std::destroy_at(&f);
  return result;
}

int Read(sqlite3_file *file, void *data, int bytes, sqlite3_int64 offset) {
  const auto &f = Of(file);
  return f.Native->pMethods->xRead(f.Native, data, bytes, offset);
}

int Write(sqlite3_file *file, const void *data, int bytes, sqlite3_int64 offset) {
  const auto &f = Of(file);
  if (bytes < 0 || offset < 0 || offset > std::numeric_limits<sqlite3_int64>::max() - bytes) {
    return SQLITE_IOERR_WRITE;
  }
  sqlite3_int64 previous = 0;
  const int sized = f.Native->pMethods->xFileSize(f.Native, &previous);
  if (sized != SQLITE_OK) { return sized; }
  const auto end = offset + bytes;
  if (end <= previous) { return f.Native->pMethods->xWrite(f.Native, data, bytes, offset); }
  State::Mutation mutation(*f.Owner);
  const int admitted = mutation.Admit(f.Path, static_cast<uint64_t>(end), f.Wal);
  return admitted == SQLITE_OK ? f.Native->pMethods->xWrite(f.Native, data, bytes, offset)
                               : admitted;
}

int Truncate(sqlite3_file *file, sqlite3_int64 bytes) {
  const auto &f = Of(file);
  if (bytes < 0) { return SQLITE_IOERR_TRUNCATE; }
  sqlite3_int64 previous = 0;
  const int sized = f.Native->pMethods->xFileSize(f.Native, &previous);
  if (sized != SQLITE_OK) { return sized; }
  if (bytes <= previous) { return f.Native->pMethods->xTruncate(f.Native, bytes); }
  State::Mutation mutation(*f.Owner);
  const int admitted = mutation.Admit(f.Path, static_cast<uint64_t>(bytes), f.Wal);
  return admitted == SQLITE_OK ? f.Native->pMethods->xTruncate(f.Native, bytes) : admitted;
}

int Sync(sqlite3_file *file, int flags) {
  const auto &f = Of(file);
  return f.Native->pMethods->xSync(f.Native, flags);
}

int Size(sqlite3_file *file, sqlite3_int64 *size) {
  const auto &f = Of(file);
  return f.Native->pMethods->xFileSize(f.Native, size);
}

int Lock(sqlite3_file *file, int level) {
  const auto &f = Of(file);
  return f.Native->pMethods->xLock(f.Native, level);
}

int Unlock(sqlite3_file *file, int level) {
  const auto &f = Of(file);
  return f.Native->pMethods->xUnlock(f.Native, level);
}

int Reserved(sqlite3_file *file, int *result) {
  const auto &f = Of(file);
  return f.Native->pMethods->xCheckReservedLock(f.Native, result);
}

int Control(sqlite3_file *file, int operation, void *argument) {
  if (operation == SQLITE_FCNTL_SIZE_HINT || operation == SQLITE_FCNTL_CHUNK_SIZE) {
    return SQLITE_OK;
  }
  const auto &f = Of(file);
  return f.Native->pMethods->xFileControl(f.Native, operation, argument);
}

int Sector(sqlite3_file *file) {
  const auto &f = Of(file);
  return f.Native->pMethods->xSectorSize(f.Native);
}

int Characteristics(sqlite3_file *file) {
  const auto &f = Of(file);
  return f.Native->pMethods->xDeviceCharacteristics(f.Native);
}

int ShmMap(sqlite3_file *file, int region, int bytes, int extend, volatile void **output) {
  const auto &f = Of(file);
  if (region < 0 || bytes < 0) { return SQLITE_IOERR_SHMSIZE; }
  if (extend == 0) { return f.Native->pMethods->xShmMap(f.Native, region, bytes, extend, output); }
  const auto end = (static_cast<uint64_t>(region) + 1) * static_cast<uint64_t>(bytes);
  State::Mutation mutation(*f.Owner);
  const int admitted = mutation.Admit(f.Path.string() + "-shm", end, true);
  return admitted == SQLITE_OK
             ? f.Native->pMethods->xShmMap(f.Native, region, bytes, extend, output)
             : admitted;
}

int ShmLock(sqlite3_file *file, int offset, int count, int flags) {
  const auto &f = Of(file);
  return f.Native->pMethods->xShmLock(f.Native, offset, count, flags);
}

void ShmBarrier(sqlite3_file *file) {
  const auto &f = Of(file);
  f.Native->pMethods->xShmBarrier(f.Native);
}

int ShmUnmap(sqlite3_file *file, int erase) {
  const auto &f = Of(file);
  return f.Native->pMethods->xShmUnmap(f.Native, erase);
}

int Fetch(sqlite3_file *file, sqlite3_int64 offset, int bytes, void **output) {
  const auto &f = Of(file);
  if (f.Native->pMethods->xFetch == nullptr) {
    *output = nullptr;
    return SQLITE_OK;
  }
  return f.Native->pMethods->xFetch(f.Native, offset, bytes, output);
}

int Unfetch(sqlite3_file *file, sqlite3_int64 offset, void *data) {
  const auto &f = Of(file);
  return f.Native->pMethods->xUnfetch == nullptr
             ? SQLITE_OK
             : f.Native->pMethods->xUnfetch(f.Native, offset, data);
}

const sqlite3_io_methods Methods{.iVersion = 3,
                                 .xClose = Close,
                                 .xRead = Read,
                                 .xWrite = Write,
                                 .xTruncate = Truncate,
                                 .xSync = Sync,
                                 .xFileSize = Size,
                                 .xLock = Lock,
                                 .xUnlock = Unlock,
                                 .xCheckReservedLock = Reserved,
                                 .xFileControl = Control,
                                 .xSectorSize = Sector,
                                 .xDeviceCharacteristics = Characteristics,
                                 .xShmMap = ShmMap,
                                 .xShmLock = ShmLock,
                                 .xShmBarrier = ShmBarrier,
                                 .xShmUnmap = ShmUnmap,
                                 .xFetch = Fetch,
                                 .xUnfetch = Unfetch};

int Delete(sqlite3_vfs *vfs, const char *name, int sync) {
  auto *p = State::From(vfs).Parent;
  return p->xDelete(p, name, sync);
}

int Access(sqlite3_vfs *vfs, const char *name, int flags, int *result) {
  auto *p = State::From(vfs).Parent;
  return p->xAccess(p, name, flags, result);
}

int FullPath(sqlite3_vfs *vfs, const char *name, int bytes, char *output) {
  auto *p = State::From(vfs).Parent;
  return p->xFullPathname(p, name, bytes, output);
}

void *DlOpen(sqlite3_vfs *vfs, const char *name) {
  auto *p = State::From(vfs).Parent;
  return p->xDlOpen(p, name);
}

void DlError(sqlite3_vfs *vfs, int bytes, char *output) {
  auto *p = State::From(vfs).Parent;
  p->xDlError(p, bytes, output);
}

using DynamicSymbol = void (*)();

DynamicSymbol DlSymbol(sqlite3_vfs *vfs, void *handle, const char *name) {
  auto *p = State::From(vfs).Parent;
  return p->xDlSym(p, handle, name);
}

void DlClose(sqlite3_vfs *vfs, void *handle) {
  auto *p = State::From(vfs).Parent;
  p->xDlClose(p, handle);
}

int Random(sqlite3_vfs *vfs, int bytes, char *output) {
  auto *p = State::From(vfs).Parent;
  return p->xRandomness(p, bytes, output);
}

int Sleep(sqlite3_vfs *vfs, int microseconds) {
  auto *p = State::From(vfs).Parent;
  return p->xSleep(p, microseconds);
}

int Time(sqlite3_vfs *vfs, double *output) {
  auto *p = State::From(vfs).Parent;
  return p->xCurrentTime(p, output);
}

int Error(sqlite3_vfs *vfs, int bytes, char *output) {
  auto *p = State::From(vfs).Parent;
  return p->xGetLastError(p, bytes, output);
}

int Time64(sqlite3_vfs *vfs, sqlite3_int64 *output) {
  auto *p = State::From(vfs).Parent;
  return p->xCurrentTimeInt64(p, output);
}

int SetCall(sqlite3_vfs *vfs, const char *name, sqlite3_syscall_ptr call) {
  auto *p = State::From(vfs).Parent;
  return p->xSetSystemCall == nullptr ? SQLITE_NOTFOUND : p->xSetSystemCall(p, name, call);
}

sqlite3_syscall_ptr GetCall(sqlite3_vfs *vfs, const char *name) {
  auto *p = State::From(vfs).Parent;
  return p->xGetSystemCall == nullptr ? nullptr : p->xGetSystemCall(p, name);
}

const char *NextCall(sqlite3_vfs *vfs, const char *name) {
  auto *p = State::From(vfs).Parent;
  return p->xNextSystemCall == nullptr ? nullptr : p->xNextSystemCall(p, name);
}
}

SqliteFileBudget::State &SqliteFileBudget::State::From(sqlite3_vfs *vfs) {
  return *static_cast<State *>(vfs->pAppData);
}

int SqliteFileBudget::State::OpenFile(
    sqlite3_vfs *vfs, sqlite3_filename name, sqlite3_file *into, int flags, int *outFlags) {
  const auto &state = State::From(vfs);
  auto *file = std::construct_at(reinterpret_cast<File *>(into));
  file->pMethods = nullptr;
  file->Owner = state.Self.lock();
  file->Native =
      reinterpret_cast<sqlite3_file *>(reinterpret_cast<std::byte *>(file) + sizeof(File));
  file->Native->pMethods = nullptr;
  file->Temporary = name == nullptr;
  if (name == nullptr) {
    uint64_t serial = 0;
    sqlite3_randomness(sizeof(serial), &serial);
    file->Path = state.Root / (".outshine-sqlite-tmp-" + std::to_string(serial));
    const auto path = file->Path.string();
    file->OwnedName = sqlite3_create_filename(path.c_str(), "", "", 0, nullptr);
    name = file->OwnedName;
    flags = static_cast<int>(static_cast<unsigned>(flags) &
                             ~static_cast<unsigned>(SQLITE_OPEN_DELETEONCLOSE));
    flags = static_cast<int>(static_cast<unsigned>(flags) |
                             static_cast<unsigned>(SQLITE_OPEN_EXCLUSIVE));
  } else {
    std::error_code error;
    file->Path = std::filesystem::weakly_canonical(name, error);
    if (error || file->Path.parent_path() != state.Root) {
      std::destroy_at(file);
      return SQLITE_CANTOPEN;
    }
  }
  file->Wal = (static_cast<unsigned>(flags) & static_cast<unsigned>(SQLITE_OPEN_WAL)) != 0;
  const int result = name == nullptr
                         ? SQLITE_NOMEM
                         : state.Parent->xOpen(state.Parent, name, file->Native, flags, outFlags);
  if (result != SQLITE_OK) {
    if (file->Native->pMethods != nullptr) { file->Native->pMethods->xClose(file->Native); }
    if (file->OwnedName != nullptr) { sqlite3_free_filename(file->OwnedName); }
    std::destroy_at(file);
    return result;
  }
  file->Relay = Methods;
  file->Relay.iVersion = std::min(3, file->Native->pMethods->iVersion);
  file->pMethods = &file->Relay;
  return SQLITE_OK;
}

std::expected<void, std::string> SqliteFileBudget::State::Register() {
  if (Parent->szOsFile < 0 ||
      static_cast<size_t>(Parent->szOsFile) >
          static_cast<size_t>(std::numeric_limits<int>::max()) - sizeof(File)) {
    return std::unexpected("SQLite file wrapper size overflow");
  }
  Vfs.szOsFile = static_cast<int>(sizeof(File)) + Parent->szOsFile;
  Vfs.xDelete = Delete;
  Vfs.xAccess = Access;
  Vfs.xFullPathname = FullPath;
  Vfs.xDlOpen = Parent->xDlOpen != nullptr ? DlOpen : nullptr;
  Vfs.xDlError = Parent->xDlError != nullptr ? DlError : nullptr;
  Vfs.xDlSym = Parent->xDlSym != nullptr ? DlSymbol : nullptr;
  Vfs.xDlClose = Parent->xDlClose != nullptr ? DlClose : nullptr;
  Vfs.xRandomness = Random;
  Vfs.xSleep = Sleep;
  Vfs.xCurrentTime = Time;
  Vfs.xGetLastError = Parent->xGetLastError != nullptr ? Error : nullptr;
  Vfs.xCurrentTimeInt64 = Parent->xCurrentTimeInt64 != nullptr ? Time64 : nullptr;
  Vfs.xSetSystemCall = SetCall;
  Vfs.xGetSystemCall = GetCall;
  Vfs.xNextSystemCall = NextCall;
  if (sqlite3_vfs_register(&Vfs, 0) != SQLITE_OK) {
    return std::unexpected("cannot register asset file budget");
  }
  Registered = true;
  return {};
}
}
