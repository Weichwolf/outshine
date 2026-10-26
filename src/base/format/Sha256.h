#ifndef OUTSHINE_BASE_FORMAT_SHA256_H
#define OUTSHINE_BASE_FORMAT_SHA256_H

#include <string_view>
#include <array>
#include <cstdint>
#include <cstddef>
#include <string>

#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#endif

namespace outshine {

class Sha256Prefix {
public:
  Sha256Prefix(const void *data, size_t bytes) noexcept;
  void Append(const void *data, size_t bytes) noexcept;
  [[nodiscard]] std::array<uint8_t, 32> Digest(const void *data, size_t bytes) const noexcept;
  [[nodiscard]] std::string Hex(const void *data, size_t bytes) const;

private:
#ifdef __APPLE__
  CC_SHA256_CTX State_{};
#else
  std::array<uint32_t, 8> State_{};
  std::array<uint8_t, 64> BufferedBytes_{};
  uint64_t Bytes_ = 0;
  size_t Buffered_ = 0;
#endif
};

[[nodiscard]] std::array<uint8_t, 32> Sha256Digest(const void *data, size_t bytes);
[[nodiscard]] std::string Sha256Hex(const void *data, size_t bytes);
[[nodiscard]] std::string Sha256Hex(std::string_view text);

}
#endif
