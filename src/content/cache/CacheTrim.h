#ifndef OUTSHINE_CONTENT_CACHE_CACHETRIM_H
#define OUTSHINE_CONTENT_CACHE_CACHETRIM_H

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>

namespace outshine {

struct CacheTrimOptions {
  uint64_t BudgetBytes = 0;
  bool Apply = false;
  bool Compact = false;
};

struct CacheTrimResult {
  uint64_t StoredBytes = 0;
  uint64_t RetainedBytes = 0;
  uint64_t EntriesRemoved = 0;
  uint64_t PackagesRemoved = 0;
  uint64_t UnknownUse = 0;
  uint64_t FileBytesBefore = 0;
  uint64_t FileBytesAfter = 0;
};

[[nodiscard]] std::expected<CacheTrimResult, std::string>
TrimAssetCaches(std::span<const std::filesystem::path> paths, CacheTrimOptions options);
[[nodiscard]] std::expected<CacheTrimResult, std::string>
TrimSourceCache(const std::filesystem::path &directory, CacheTrimOptions options);

}
#endif
