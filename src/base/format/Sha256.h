#ifndef OUTSHINE_BASE_FORMAT_SHA256_H
#define OUTSHINE_BASE_FORMAT_SHA256_H

#include <string_view>
#include <cstddef>
#include <string>

namespace outshine {

[[nodiscard]] std::string Sha256Hex(const void *data, size_t bytes);
[[nodiscard]] std::string Sha256Hex(std::string_view text);

} // namespace outshine
#endif
