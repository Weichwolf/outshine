#include "AssetCacheState.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>
#include <zlib.h>

namespace outshine {

CachedAsset::CachedAsset(AssetRecord record, std::shared_ptr<const std::vector<uint8_t>> bytes)
    : Record_(std::move(record)), Package_(std::move(bytes)) {}

std::span<const uint8_t> CachedAsset::Bytes() const noexcept {
  return std::span(*Package_).subspan(static_cast<size_t>(Record_.OffsetBytes),
                                      static_cast<size_t>(Record_.ByteCount));
}

std::expected<std::optional<CachedAsset>, AssetCacheError>
AssetCache::Load(std::string_view key, size_t packageBytesMost) const {
  auto found = Find(key);
  if (!found) { return std::unexpected(found.error()); }
  if (!*found) { return std::optional<CachedAsset>{}; }
  const auto &record = **found;
  AssetSql::Statement statement;
  auto prepared = AssetSql::Prepare(State_->Database,
                                    "SELECT rowid,length(bytes),crc FROM packages "
                                    "WHERE key=? AND typeof(bytes)='blob'",
                                    statement);
  if (!prepared) { return std::unexpected(prepared.error()); }
  if (!AssetSql::Text(statement.Value, 1, record.Package)) {
    return std::unexpected(AssetCacheError::Storage);
  }
  const int code = sqlite3_step(statement.Value);
  if (code == SQLITE_DONE) { return std::optional<CachedAsset>{}; }
  if (code != SQLITE_ROW) { return std::unexpected(AssetSql::Error(code)); }
  const auto count = sqlite3_column_int64(statement.Value, 1);
  if (count < 0) { return std::optional<CachedAsset>{}; }
  if (std::cmp_greater(count, packageBytesMost)) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  const auto bytes = static_cast<size_t>(count);
  if (record.OffsetBytes > bytes || record.ByteCount > bytes - record.OffsetBytes) {
    return std::optional<CachedAsset>{};
  }
  sqlite3_blob *raw = nullptr;
  const int opened = sqlite3_blob_open(State_->Database,
                                       "main",
                                       "packages",
                                       "bytes",
                                       sqlite3_column_int64(statement.Value, 0),
                                       0,
                                       &raw);
  const std::unique_ptr<sqlite3_blob, decltype(&sqlite3_blob_close)> input(raw,
                                                                           &sqlite3_blob_close);
  if (opened != SQLITE_OK) { return std::unexpected(AssetSql::Error(opened)); }
  if (std::cmp_not_equal(sqlite3_blob_bytes(input.get()), bytes)) {
    return std::optional<CachedAsset>{};
  }
  auto package = std::make_shared<std::vector<uint8_t>>(bytes);
  if (bytes != 0) {
    const int read = sqlite3_blob_read(input.get(), package->data(), static_cast<int>(bytes), 0);
    if (read != SQLITE_OK) { return std::unexpected(AssetSql::Error(read)); }
  }
  if (std::cmp_not_equal(sqlite3_column_int64(statement.Value, 2),
                         crc32_z(0, package->data(), package->size()))) {
    return std::optional<CachedAsset>{};
  }
  return CachedAsset(std::move(**found), std::move(package));
}

}
