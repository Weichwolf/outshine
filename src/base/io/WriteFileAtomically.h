#ifndef OUTSHINE_BASE_IO_WRITEFILEATOMICALLY_H
#define OUTSHINE_BASE_IO_WRITEFILEATOMICALLY_H
#include <expected>
#include <span>
#include <cstddef>
#include <string>
#include <string_view>

namespace outshine {
[[nodiscard]] std::expected<void, std::string>
WriteFileAtomically(std::string_view path, std::span<const std::byte> bytes);
}
#endif
