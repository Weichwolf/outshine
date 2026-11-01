#ifndef OUTSHINE_STORAGE_FILEBUDGETSYSTEM_H
#define OUTSHINE_STORAGE_FILEBUDGETSYSTEM_H
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace outshine::FileBudgetSystem {
struct Size {
  uint64_t Bytes;
  uint64_t Allocated;
};

[[nodiscard]] int OpenLock(const std::filesystem::path &path);
void Close(int fd);
[[nodiscard]] bool Lock(int fd, bool exclusive, bool wait);
void Unlock(int fd);
[[nodiscard]] std::expected<std::optional<Size>, std::string>
Inspect(const std::filesystem::path &path);
[[nodiscard]] std::expected<uint64_t, std::string> BlockBytes(const std::filesystem::path &root);
}
#endif
