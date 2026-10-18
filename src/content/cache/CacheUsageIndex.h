#ifndef OUTSHINE_CONTENT_CACHE_CACHEUSAGEINDEX_H
#define OUTSHINE_CONTENT_CACHE_CACHEUSAGEINDEX_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace outshine {

class CacheUsageIndex {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<CacheUsageIndex>, std::string>
  Open(const std::string &path);
  ~CacheUsageIndex();
  CacheUsageIndex(const CacheUsageIndex &) = delete;
  CacheUsageIndex &operator=(const CacheUsageIndex &) = delete;

  [[nodiscard]] std::expected<void, std::string>
  Record(std::string_view key, uint64_t storedBytes, bool published = false);
  [[nodiscard]] std::expected<void, std::string> Flush();
  [[nodiscard]] size_t Pending() const;

private:
  struct State;
  explicit CacheUsageIndex(std::unique_ptr<State> state);
  std::unique_ptr<State> State_;
};

}
#endif
