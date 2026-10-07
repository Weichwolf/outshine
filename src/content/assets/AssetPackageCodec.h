#ifndef OUTSHINE_CONTENT_ASSETS_ASSETPACKAGECODEC_H
#define OUTSHINE_CONTENT_ASSETS_ASSETPACKAGECODEC_H

#include "content/AssetCache.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace outshine::AssetSql {

[[nodiscard]] std::expected<std::vector<uint8_t>, AssetCacheError>
Compress(std::span<const uint8_t> bytes);
[[nodiscard]] std::optional<std::vector<uint8_t>>
Decompress(std::vector<uint8_t> bytes, int codec, size_t nativeBytes);

}
#endif
