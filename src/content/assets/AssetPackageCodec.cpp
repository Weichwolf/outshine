#include "AssetPackageCodec.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

namespace outshine::AssetSql {
static_assert(alignof(uint64_t) >= 8);

namespace {
struct CompressionStorage {
  size_t Output = 0;
  size_t WorkspaceWords = 0;
};

std::expected<CompressionStorage, AssetCacheError> PlanCompression(size_t bytes) {
  if (bytes < 64) { return CompressionStorage{}; }
  const size_t output = ZSTD_compressBound(bytes);
  const auto parameters = ZSTD_getCParams(1, bytes, 0);
  const size_t workspace = ZSTD_estimateCCtxSize_usingCParams(parameters);
  const size_t workspaceMost = ZSTD_estimateCCtxSize(1);
  constexpr size_t word = sizeof(uint64_t);
  if (ZSTD_isError(output) != 0 || ZSTD_isError(workspace) != 0 ||
      ZSTD_isError(workspaceMost) != 0 || workspace > workspaceMost ||
      workspace > std::numeric_limits<size_t>::max() - (word - 1)) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  const size_t words = (workspace + word - 1) / word;
  if (words > (std::numeric_limits<size_t>::max() - output) / word) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  return CompressionStorage{.Output = output, .WorkspaceWords = words};
}
}

std::expected<size_t, AssetCacheError> CompressionBytes(size_t nativeBytes) {
  if (nativeBytes < 64) { return size_t{0}; }
  const size_t output = ZSTD_compressBound(nativeBytes);
  const size_t workspace = ZSTD_estimateCCtxSize(1);
  if (ZSTD_isError(output) != 0 || ZSTD_isError(workspace) != 0 ||
      workspace > std::numeric_limits<size_t>::max() - (sizeof(uint64_t) - 1)) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  const size_t words = (workspace + sizeof(uint64_t) - 1) / sizeof(uint64_t);
  if (words > (std::numeric_limits<size_t>::max() - output) / sizeof(uint64_t)) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  return output + words * sizeof(uint64_t);
}

std::expected<std::vector<uint8_t>, AssetCacheError> Compress(std::span<const uint8_t> bytes) {
  if (bytes.size() < 64) { return std::vector<uint8_t>{}; }
  const auto storage = PlanCompression(bytes.size());
  if (!storage) { return std::unexpected(storage.error()); }
  std::vector<uint64_t> workspace(storage->WorkspaceWords);
  auto *context = ZSTD_initStaticCCtx(workspace.data(), workspace.size() * sizeof(uint64_t));
  if (context == nullptr) { return std::unexpected(AssetCacheError::Storage); }
  std::vector<uint8_t> encoded(storage->Output);
  const size_t count =
      ZSTD_compressCCtx(context, encoded.data(), encoded.size(), bytes.data(), bytes.size(), 1);
  if (ZSTD_isError(count) != 0) { return std::unexpected(AssetCacheError::Storage); }
  if (count >= bytes.size()) { return std::vector<uint8_t>{}; }
  encoded.resize(count);
  return encoded;
}

std::expected<size_t, AssetCacheError>
DecompressionBytes(size_t storedBytes, int codec, size_t nativeBytes) {
  if (codec == 0) {
    if (storedBytes != nativeBytes) { return std::unexpected(AssetCacheError::InvalidInput); }
    return storedBytes;
  }
  if (codec != 1 || storedBytes == 0 || nativeBytes <= storedBytes) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  const size_t workspace = ZSTD_estimateDCtxSize();
  constexpr size_t word = sizeof(uint64_t);
  if (ZSTD_isError(workspace) != 0 || workspace > std::numeric_limits<size_t>::max() - (word - 1)) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  const size_t aligned = (workspace + word - 1) / word * word;
  if (nativeBytes > std::numeric_limits<size_t>::max() - aligned ||
      storedBytes > std::numeric_limits<size_t>::max() - aligned - nativeBytes) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  return storedBytes + nativeBytes + aligned;
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
  const auto plan = DecompressionBytes(bytes.size(), codec, nativeBytes);
  if (!plan) { return std::nullopt; }
  const size_t workspaceBytes = *plan - bytes.size() - nativeBytes;
  std::vector<uint64_t> workspace(workspaceBytes / sizeof(uint64_t));
  auto *context = ZSTD_initStaticDCtx(workspace.data(), workspaceBytes);
  if (context == nullptr) { return std::nullopt; }
  std::vector<uint8_t> decoded(nativeBytes);
  const size_t count =
      ZSTD_decompressDCtx(context, decoded.data(), decoded.size(), bytes.data(), bytes.size());
  if (ZSTD_isError(count) != 0 || count != nativeBytes) { return std::nullopt; }
  return decoded;
}

}
