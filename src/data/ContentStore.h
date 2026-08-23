#ifndef OUTSHINE_DATA_CONTENTSTORE_H
#define OUTSHINE_DATA_CONTENTSTORE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Address.h"
#include "SourceDecl.h"

namespace outshine::Data {

[[nodiscard]] std::string ContentKey(const SourceDecl &decl, const Address &at);

class ContentStore {
public:
  enum class Use { Off, On };

  struct Config {

    std::string Directory;
    Use Using = Use::On;

    size_t CapBytes = 0;
  };

  explicit ContentStore(const Config &config);

  ContentStore(const ContentStore &) = delete;
  ContentStore &operator=(const ContentStore &) = delete;

  [[nodiscard]] bool TryRead(std::string_view key, std::vector<uint8_t> *out) const;
  void Keep(std::string_view key, const uint8_t *data, size_t bytes);

  [[nodiscard]] const std::string &Directory() const noexcept { return Directory_; }
  [[nodiscard]] bool Enabled() const noexcept { return Using_ == Use::On; }

  struct Ledger {
    long long Hits = 0, Misses = 0, Writes = 0, WriteFailures = 0, Swept = 0;
    long long SweptBytes = 0;
  };
  [[nodiscard]] Ledger Counters() const;

private:
  std::string Directory_;
  Use Using_;
  size_t CapBytes_;

  mutable std::atomic<long long> Hits_{0}, Misses_{0};
  std::atomic<long long> Writes_{0}, WriteFailures_{0};
  long long Swept_ = 0, SweptBytes_ = 0;
  std::atomic<uint64_t> TempSerial_{0};
};

}
#endif
