#ifndef OUTSHINE_BASE_FORMAT_SHA256_H
#define OUTSHINE_BASE_FORMAT_SHA256_H

#include <string_view>
#include <array>
#include <cstdint>
#include <cstddef>
#include <string>

namespace outshine {

[[nodiscard]] std::array<uint8_t, 32> Sha256Digest(const void *data, size_t bytes);
[[nodiscard]] std::string Sha256Hex(const void *data, size_t bytes);
[[nodiscard]] std::string Sha256Hex(std::string_view text);

}
#endif
