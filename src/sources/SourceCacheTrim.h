#ifndef OUTSHINE_SOURCES_SOURCECACHETRIM_H
#define OUTSHINE_SOURCES_SOURCECACHETRIM_H

#include "CacheTrim.h"
#include <expected>
#include <filesystem>
#include <string>

namespace outshine {
[[nodiscard]] std::expected<CacheTrimResult, std::string>
TrimSourceCache(const std::filesystem::path &directory, CacheTrimOptions options);
}
#endif
