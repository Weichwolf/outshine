#include "FileBudgetSystem.h"
#include <cerrno>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#include <vector>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#endif

namespace outshine::FileBudgetSystem {
constexpr auto kOwnerReadWrite = 0600;
#ifdef _WIN32
int OpenLock(const std::filesystem::path &path) {
  return _wopen(path.c_str(), _O_RDWR | _O_CREAT | _O_BINARY | _O_NOINHERIT, _S_IREAD | _S_IWRITE);
}

void Close(int fd) {
  (void)_close(fd);
}

bool Lock(int fd, bool exclusive, bool wait) {
  OVERLAPPED at{};
  const DWORD flags =
      (exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0) | (wait ? 0 : LOCKFILE_FAIL_IMMEDIATELY);
  return LockFileEx(reinterpret_cast<HANDLE>(_get_osfhandle(fd)), flags, 0, 1, 0, &at) != 0;
}

void Unlock(int fd) {
  OVERLAPPED at{};
  (void)UnlockFileEx(reinterpret_cast<HANDLE>(_get_osfhandle(fd)), 0, 1, 0, &at);
}

std::expected<std::optional<Size>, std::string> Inspect(const std::filesystem::path &path) {
  const auto handle = CreateFileW(path.c_str(),
                                  FILE_READ_ATTRIBUTES,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr,
                                  OPEN_EXISTING,
                                  0,
                                  nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    if (GetLastError() == ERROR_FILE_NOT_FOUND) { return std::optional<Size>{}; }
    return std::unexpected("cannot inspect asset allocation");
  }
  FILE_STANDARD_INFO info{};
  const bool read =
      GetFileInformationByHandleEx(handle, FileStandardInfo, &info, sizeof(info)) != 0;
  (void)CloseHandle(handle);
  if (!read || info.Directory || info.NumberOfLinks != 1 || info.EndOfFile.QuadPart < 0 ||
      info.AllocationSize.QuadPart < 0) {
    return std::unexpected("asset budget requires a regular file allocation");
  }
  return Size{static_cast<uint64_t>(info.EndOfFile.QuadPart),
              static_cast<uint64_t>(info.AllocationSize.QuadPart)};
}

std::expected<uint64_t, std::string> BlockBytes(const std::filesystem::path &root) {
  std::vector<wchar_t> volume(32768);
  DWORD sectors = 0, bytes = 0, free = 0, total = 0;
  if (!GetVolumePathNameW(root.c_str(), volume.data(), static_cast<DWORD>(volume.size())) ||
      !GetDiskFreeSpaceW(volume.data(), &sectors, &bytes, &free, &total) || sectors == 0 ||
      bytes == 0) {
    return std::unexpected("cannot identify asset allocation block size");
  }
  return static_cast<uint64_t>(sectors) * bytes;
}
#else
int OpenLock(const std::filesystem::path &path) {
  return open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, kOwnerReadWrite);
}

void Close(int fd) {
  (void)close(fd);
}

bool Lock(int fd, bool exclusive, bool wait) {
  const auto flags = static_cast<unsigned>(exclusive ? LOCK_EX : LOCK_SH) |
                     static_cast<unsigned>(wait ? 0 : LOCK_NB);
  return flock(fd, static_cast<int>(flags)) == 0;
}

void Unlock(int fd) {
  (void)flock(fd, LOCK_UN);
}

std::expected<std::optional<Size>, std::string> Inspect(const std::filesystem::path &path) {
  struct stat info{};
  if (stat(path.c_str(), &info) != 0) {
    if (errno == ENOENT) { return std::optional<Size>{}; }
    return std::unexpected("cannot stat asset budget file");
  }
  if (!S_ISREG(info.st_mode) || info.st_nlink != 1 || info.st_size < 0 || info.st_blocks < 0 ||
      static_cast<uint64_t>(info.st_blocks) > std::numeric_limits<uint64_t>::max() / 512) {
    return std::unexpected("asset budget requires a regular file allocation");
  }
  return Size{.Bytes = static_cast<uint64_t>(info.st_size),
              .Allocated = static_cast<uint64_t>(info.st_blocks) * 512};
}

std::expected<uint64_t, std::string> BlockBytes(const std::filesystem::path &root) {
  struct statvfs info{};
  if (statvfs(root.c_str(), &info) != 0 || info.f_frsize == 0) {
    return std::unexpected("cannot identify asset allocation block size");
  }
  return info.f_frsize;
}
#endif
}
