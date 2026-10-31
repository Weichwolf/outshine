#include "AssetCacheState.h"
#include "AssetPackageCodec.h"
#include <algorithm>
#include <cstddef>
#include <limits>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <zlib.h>

namespace outshine {
namespace {
std::expected<std::optional<std::vector<uint8_t>>, AssetCacheError>
ReadBlob(sqlite3 *database, sqlite3_int64 row, const char *table, size_t bytes) {
  sqlite3_blob *raw = nullptr;
  const int opened = sqlite3_blob_open(database, "main", table, "bytes", row, 0, &raw);
  const std::unique_ptr<sqlite3_blob, decltype(&sqlite3_blob_close)> input(raw,
                                                                           &sqlite3_blob_close);
  if (opened != SQLITE_OK) { return std::unexpected(AssetSql::Error(opened)); }
  if (std::cmp_not_equal(sqlite3_blob_bytes(input.get()), bytes)) { return std::nullopt; }
  std::vector<uint8_t> stored(bytes);
  if (bytes != 0) {
    const int read = sqlite3_blob_read(input.get(), stored.data(), static_cast<int>(bytes), 0);
    if (read != SQLITE_OK) { return std::unexpected(AssetSql::Error(read)); }
  }
  return stored;
}
}

std::expected<std::optional<CachedAsset>, AssetCacheError>
AssetCache::LoadRequest(std::string_view requestKey, size_t packageBytesMost) const {
  auto found = FindRequest(requestKey);
  if (!found) { return std::unexpected(found.error()); }
  if (!*found) { return std::optional<CachedAsset>{}; }
  auto loaded = Load((**found).Key, packageBytesMost);
  if (loaded && *loaded) { (**loaded).Record_.RequestKey = std::string(requestKey); }
  return loaded;
}

void AssetCache::UsePackage(std::string_view packageKey) const {
  const std::scoped_lock lock(*State_->Mutex);
  AssetSql::Statement statement;
  const auto prepared = AssetSql::Prepare(
      State_->Database,
      "SELECT CASE WHEN b.rowid IS NULL THEN length(p.bytes) ELSE length(b.bytes) END "
      "FROM packages p LEFT JOIN package_bodies b ON b.key=p.key WHERE p.key=? "
      "AND CASE WHEN b.rowid IS NULL THEN typeof(p.bytes) ELSE typeof(b.bytes) END='blob'",
      statement);
  if (!prepared || !AssetSql::Text(statement.Value, 1, packageKey) ||
      sqlite3_step(statement.Value) != SQLITE_ROW ||
      sqlite3_column_type(statement.Value, 0) != SQLITE_INTEGER ||
      sqlite3_column_int64(statement.Value, 0) < 0) {
    State_->WarnUsage();
    return;
  }
  const auto bytes = static_cast<uint64_t>(sqlite3_column_int64(statement.Value, 0));
  sqlite3_reset(statement.Value);
  State_->Observe(packageKey, bytes);
}

CachedAsset::CachedAsset(AssetRecord record, std::vector<uint8_t> bytes)
    : Record_(std::move(record)), Package_(std::move(bytes)) {}

std::span<const uint8_t> CachedAsset::Bytes() const noexcept {
  return std::span(Package_).subspan(static_cast<size_t>(Record_.OffsetBytes),
                                     static_cast<size_t>(Record_.ByteCount));
}

std::expected<std::optional<AssetCache::ReadPlan>, AssetCacheError>
AssetCache::PlanRead(std::string_view key, size_t packageBytesMost) const {
  if (!AssetSql::ValidKey(key) || packageBytesMost == 0) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  const std::scoped_lock lock(*State_->Mutex);
  AssetSql::Statement statement;
  auto prepared = AssetSql::Prepare(
      State_->Database,
      "SELECT CASE WHEN typeof(p.codec)='integer' AND typeof(p.native_bytes)='integer' "
      "THEN CASE WHEN b.rowid IS NULL THEN CASE WHEN typeof(p.bytes)='blob' THEN length(p.bytes) "
      "END "
      "ELSE CASE WHEN typeof(b.bytes)='blob' THEN length(b.bytes) END END "
      "END,p.codec,p.native_bytes "
      "FROM assets a LEFT JOIN packages p ON p.key=a.package "
      "LEFT JOIN package_bodies b ON b.key=p.key WHERE a.key=?",
      statement);
  if (!prepared) { return std::unexpected(prepared.error()); }
  if (!AssetSql::Text(statement.Value, 1, key)) {
    return std::unexpected(AssetCacheError::Storage);
  }
  const int code = sqlite3_step(statement.Value);
  if (code == SQLITE_DONE) { return std::nullopt; }
  if (code != SQLITE_ROW) { return std::unexpected(AssetSql::Error(code)); }
  if (sqlite3_column_type(statement.Value, 0) != SQLITE_INTEGER) { return ReadPlan{}; }
  const auto stored = sqlite3_column_int64(statement.Value, 0);
  const auto codec = sqlite3_column_int64(statement.Value, 1);
  const auto declared = sqlite3_column_int64(statement.Value, 2);
  if (stored < 0 || declared < 0 || codec < 0 || codec > 1) { return ReadPlan{}; }
  const auto native = codec == 0 && declared == 0 ? stored : declared;
  if (std::cmp_greater(stored, packageBytesMost) || std::cmp_greater(native, packageBytesMost)) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  const auto peak = AssetSql::DecompressionBytes(
      static_cast<size_t>(stored), static_cast<int>(codec), static_cast<size_t>(native));
  if (!peak) {
    if (peak.error() == AssetCacheError::InvalidInput) { return ReadPlan{}; }
    return std::unexpected(peak.error());
  }
  return ReadPlan{.PackageBytesMost = static_cast<size_t>(std::max(stored, native)),
                  .BufferBytesMost = *peak};
}

std::expected<std::optional<CachedAsset>, AssetCacheError>
AssetCache::Load(std::string_view key, size_t packageBytesMost) const {
  return Load(key,
              ReadPlan{.PackageBytesMost = packageBytesMost,
                       .BufferBytesMost = std::numeric_limits<size_t>::max()});
}

std::expected<std::optional<CachedAsset>, AssetCacheError>
AssetCache::Load(std::string_view key, const ReadPlan &plan) const {
  const auto packageBytesMost = plan.PackageBytesMost;
  std::unique_lock lock(*State_->Mutex);
  auto found = Find(key);
  if (!found) { return std::unexpected(found.error()); }
  if (!*found) { return std::optional<CachedAsset>{}; }
  const auto &record = **found;
  AssetSql::Statement statement;
  auto prepared = AssetSql::Prepare(
      State_->Database,
      "SELECT CASE WHEN b.rowid IS NULL THEN p.rowid ELSE b.rowid END,"
      "CASE WHEN b.rowid IS NULL THEN length(p.bytes) ELSE length(b.bytes) END,"
      "p.crc,p.codec,p.native_bytes,b.rowid IS NOT NULL FROM packages p "
      "LEFT JOIN package_bodies b ON b.key=p.key WHERE p.key=? "
      "AND typeof(p.codec)='integer' AND typeof(p.native_bytes)='integer' "
      "AND CASE WHEN b.rowid IS NULL THEN typeof(p.bytes) ELSE typeof(b.bytes) END='blob'",
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
  if (count < 0 || declared < 0 || codec < 0 || codec > 1) { return std::optional<CachedAsset>{}; }
  const auto native = codec == 0 && declared == 0 ? count : declared;
  if (std::cmp_greater(count, packageBytesMost) || std::cmp_greater(native, packageBytesMost)) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  const auto memory = AssetSql::DecompressionBytes(
      static_cast<size_t>(count), static_cast<int>(codec), static_cast<size_t>(native));
  if (!memory) {
    if (memory.error() == AssetCacheError::InvalidInput) { return std::optional<CachedAsset>{}; }
    return std::unexpected(memory.error());
  }
  if (*memory > plan.BufferBytesMost) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  const auto bytes = static_cast<size_t>(count);
  if (std::cmp_greater(record.OffsetBytes, native) ||
      record.ByteCount > static_cast<uint64_t>(native) - record.OffsetBytes) {
    return std::optional<CachedAsset>{};
  }
  const auto *table = sqlite3_column_int(statement.Value, 5) != 0 ? "package_bodies" : "packages";
  auto stored = ReadBlob(State_->Database, sqlite3_column_int64(statement.Value, 0), table, bytes);
  if (!stored) { return std::unexpected(stored.error()); }
  if (!*stored) { return std::optional<CachedAsset>{}; }
  const auto expectedCrc = sqlite3_column_int64(statement.Value, 2);
  sqlite3_finalize(std::exchange(statement.Value, nullptr));
  lock.unlock();
  if (std::cmp_not_equal(expectedCrc, crc32_z(0, (**stored).data(), (**stored).size()))) {
    return std::optional<CachedAsset>{};
  }
  auto decoded = AssetSql::Decompress(
      std::move(**stored), static_cast<int>(codec), static_cast<size_t>(native));
  if (!decoded) { return std::optional<CachedAsset>{}; }
  lock.lock();
  State_->Observe(record.Package, bytes);
  return CachedAsset(std::move(**found), std::move(*decoded));
}

}
