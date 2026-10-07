#include "AssetCacheState.h"
#include "AssetPackageCodec.h"
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
  auto prepared =
      AssetSql::Prepare(State_->Database,
                        "SELECT rowid,length(bytes),crc,codec,native_bytes FROM packages "
                        "WHERE key=? AND typeof(bytes)='blob' AND typeof(codec)='integer' "
                        "AND typeof(native_bytes)='integer'",
                        statement);
  if (!prepared) { return std::unexpected(prepared.error()); }
  if (!AssetSql::Text(statement.Value, 1, record.Package)) {
    return std::unexpected(AssetCacheError::Storage);
  }
  const int code = sqlite3_step(statement.Value);
  if (code == SQLITE_DONE) { return std::optional<CachedAsset>{}; }
  if (code != SQLITE_ROW) { return std::unexpected(AssetSql::Error(code)); }
  const auto count = sqlite3_column_int64(statement.Value, 1);
  const auto codec = sqlite3_column_int64(statement.Value, 3);
  const auto declared = sqlite3_column_int64(statement.Value, 4);
  if (count < 0 || declared < 0 || (codec != 0 && codec != 1)) {
    return std::optional<CachedAsset>{};
  }
  const auto native = codec == 0 && declared == 0 ? count : declared;
  if (std::cmp_greater(count, packageBytesMost) || std::cmp_greater(native, packageBytesMost)) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  const auto bytes = static_cast<size_t>(count);
  if (std::cmp_greater(record.OffsetBytes, native) ||
      record.ByteCount > static_cast<uint64_t>(native) - record.OffsetBytes) {
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
  std::vector<uint8_t> stored(bytes);
  if (bytes != 0) {
    const int read = sqlite3_blob_read(input.get(), stored.data(), static_cast<int>(bytes), 0);
    if (read != SQLITE_OK) { return std::unexpected(AssetSql::Error(read)); }
  }
  if (std::cmp_not_equal(sqlite3_column_int64(statement.Value, 2),
                         crc32_z(0, stored.data(), stored.size()))) {
    return std::optional<CachedAsset>{};
  }
  auto decoded =
      AssetSql::Decompress(std::move(stored), static_cast<int>(codec), static_cast<size_t>(native));
  if (!decoded) { return std::optional<CachedAsset>{}; }
  return CachedAsset(std::move(**found),
                     std::make_shared<const std::vector<uint8_t>>(std::move(*decoded)));
}

}
