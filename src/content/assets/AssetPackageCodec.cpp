#include "AssetPackageCodec.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <utility>
#include <vector>
#include <zstd.h>

namespace outshine::AssetSql {

std::expected<std::vector<uint8_t>, AssetCacheError> Compress(std::span<const uint8_t> bytes) {
  if (bytes.size() < 64) { return std::vector<uint8_t>{}; }
  const size_t bound = ZSTD_compressBound(bytes.size());
  if (ZSTD_isError(bound) != 0) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  std::vector<uint8_t> encoded(bound);
  const size_t count = ZSTD_compress(encoded.data(), encoded.size(), bytes.data(), bytes.size(), 1);
  if (ZSTD_isError(count) != 0) { return std::unexpected(AssetCacheError::Storage); }
  if (count >= bytes.size()) { return std::vector<uint8_t>{}; }
  encoded.resize(count);
  return encoded;
}

std::optional<std::vector<uint8_t>>
Decompress(std::vector<uint8_t> bytes, int codec, size_t nativeBytes) {
  if (codec == 0) {
    if (bytes.size() != nativeBytes) { return std::nullopt; }
    return bytes;
  }
  if (codec != 1 || bytes.empty() || nativeBytes == 0 || bytes.size() >= nativeBytes ||
      ZSTD_getFrameContentSize(bytes.data(), bytes.size()) != nativeBytes ||
      ZSTD_findFrameCompressedSize(bytes.data(), bytes.size()) != bytes.size()) {
    return std::nullopt;
  }
  std::vector<uint8_t> decoded(nativeBytes);
  const size_t count = ZSTD_decompress(decoded.data(), decoded.size(), bytes.data(), bytes.size());
  if (ZSTD_isError(count) != 0 || count != nativeBytes) { return std::nullopt; }
  return decoded;
}

}
