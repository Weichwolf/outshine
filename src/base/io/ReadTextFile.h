#ifndef OUTSHINE_BASE_IO_READTEXTFILE_H
#define OUTSHINE_BASE_IO_READTEXTFILE_H
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace outshine {
[[nodiscard]] std::expected<std::string, std::string> ReadTextFile(std::string_view path,
                                                                   size_t byteLimit);
}
#endif
