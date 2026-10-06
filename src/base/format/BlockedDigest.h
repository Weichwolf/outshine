#ifndef OUTSHINE_BASE_FORMAT_BLOCKEDDIGEST_H
#define OUTSHINE_BASE_FORMAT_BLOCKEDDIGEST_H

#include "ByteArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace outshine {
class BlockedDigest {
public:
  template <class T> bool Number(T value) {
    const auto encoded = LittleEndian(value);
    return Put({reinterpret_cast<const uint8_t *>(&encoded), sizeof(T)});
  }

  bool Put(std::span<const uint8_t> bytes) {
    Count_ += bytes.size();
    while (!bytes.empty()) {
      const size_t count = std::min(bytes.size(), Block_.size() - Filled_);
      std::memcpy(Block_.data() + Filled_, bytes.data(), count);
      Filled_ += count;
      bytes = bytes.subspan(count);
      if (Filled_ == Block_.size()) { Fold(); }
    }
    return true;
  }

  [[nodiscard]] uint64_t Count() const noexcept { return Count_; }

  [[nodiscard]] std::array<uint8_t, 32> Finish() {
    if (Filled_ != 0) { Fold(); }
    return Digest_;
  }

private:
  void Fold() {
    const auto block = Sha256Digest(Block_.data(), Filled_);
    std::array<uint8_t, 64> pair;
    std::ranges::copy(Digest_, pair.begin());
    std::ranges::copy(block, pair.begin() + Digest_.size());
    Digest_ = Sha256Digest(pair.data(), pair.size());
    Filled_ = 0;
  }

  static constexpr size_t kBlockBytes = 65536;
  std::array<uint8_t, kBlockBytes> Block_;
  std::array<uint8_t, 32> Digest_{};
  size_t Filled_ = 0;
  uint64_t Count_ = 0;
};
}

#endif
