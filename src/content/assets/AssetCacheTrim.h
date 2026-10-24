#ifndef OUTSHINE_CONTENT_ASSETS_ASSETCACHETRIM_H
#define OUTSHINE_CONTENT_ASSETS_ASSETCACHETRIM_H

#include "CacheTrim.h"
#include <expected>
#include <filesystem>
#include <span>
#include <string>

namespace outshine {
[[nodiscard]] std::expected<CacheTrimResult, std::string>
TrimAssetCaches(std::span<const std::filesystem::path> paths, CacheTrimOptions options);
}
#endif
