#ifndef OUTSHINE_CONTENT_ASSETS_ASSETPACKAGECODEC_H
#define OUTSHINE_CONTENT_ASSETS_ASSETPACKAGECODEC_H

#include "AssetCache.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace outshine::AssetSql {

[[nodiscard]] std::expected<size_t, AssetCacheError> CompressionBytes(size_t nativeBytes);
[[nodiscard]] std::expected<size_t, AssetCacheError>
DecompressionBytes(size_t storedBytes, int codec, size_t nativeBytes);
[[nodiscard]] std::expected<std::vector<uint8_t>, AssetCacheError>
Compress(std::span<const uint8_t> bytes);
[[nodiscard]] std::optional<std::vector<uint8_t>>
Decompress(std::vector<uint8_t> bytes, int codec, size_t nativeBytes);

}
#endif
