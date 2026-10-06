#include "AssetCacheState.h"
#include "Sha256.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <string>
#include <vector>
#include <zlib.h>

namespace outshine {
namespace {

class Transaction {
public:
  explicit Transaction(sqlite3 *database) : Database_(database) {}

  ~Transaction() {
    if (Database_ != nullptr) { sqlite3_exec(Database_, "ROLLBACK", nullptr, nullptr, nullptr); }
  }

  Transaction(const Transaction &) = delete;
  Transaction &operator=(const Transaction &) = delete;

  [[nodiscard]] std::expected<void, AssetCacheError> Commit() {
    auto committed = AssetSql::Exec(Database_, "COMMIT");
    if (committed) { Database_ = nullptr; }
    return committed;
  }

private:
  sqlite3 *Database_;
};

bool Bind(sqlite3_stmt *statement, const AssetRecord &record) {
  bool bound =
      AssetSql::Text(statement, 1, record.Key) && AssetSql::Text(statement, 2, record.Kind) &&
      AssetSql::Text(statement, 9, record.Package) && AssetSql::Text(statement, 13, record.Parent);
  for (int axis = 0; axis < 3; ++axis) {
    const auto index = static_cast<size_t>(axis);
    bound = sqlite3_bind_double(statement, 3 + axis, record.Bounds.Min[index]) == SQLITE_OK &&
            sqlite3_bind_double(statement, 6 + axis, record.Bounds.Max[index]) == SQLITE_OK &&
            bound;
  }
  return sqlite3_bind_int64(statement, 10, static_cast<sqlite3_int64>(record.OffsetBytes)) ==
             SQLITE_OK &&
         sqlite3_bind_int64(statement, 11, static_cast<sqlite3_int64>(record.ByteCount)) ==
             SQLITE_OK &&
         sqlite3_bind_int64(statement, 12, record.Level) == SQLITE_OK && bound;
}

std::expected<void, AssetCacheError>
StorePackage(sqlite3 *database, std::string_view key, std::span<const uint8_t> payload) {
  AssetSql::Statement statement;
  const auto prepared =
      AssetSql::Prepare(database,
                        "INSERT INTO packages(key,bytes,crc) VALUES(?,?,?) ON CONFLICT(key) "
                        "DO UPDATE SET bytes=excluded.bytes,crc=excluded.crc",
                        statement);
  if (!prepared) { return prepared; }
  const auto checksum = crc32_z(0, payload.data(), payload.size());
  const int blob =
      payload.empty()
          ? sqlite3_bind_zeroblob(statement.Value, 2, 0)
          : sqlite3_bind_blob64(statement.Value, 2, payload.data(), payload.size(), SQLITE_STATIC);
  if (!AssetSql::Text(statement.Value, 1, key) || blob != SQLITE_OK ||
      sqlite3_bind_int64(statement.Value, 3, static_cast<sqlite3_int64>(checksum)) != SQLITE_OK) {
    return std::unexpected(AssetCacheError::Storage);
  }
  const int code = sqlite3_step(statement.Value);
  if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
  return {};
}

}

std::expected<void, AssetCacheError> AssetCache::Publish(std::span<const AssetRecord> records,
                                                         std::span<const uint8_t> payload) {
  if (records.empty()) { return std::unexpected(AssetCacheError::InvalidInput); }
  if (payload.size() >
      static_cast<size_t>(sqlite3_limit(State_->Database, SQLITE_LIMIT_LENGTH, -1))) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  const std::string key = Sha256Hex(payload.data(), payload.size());
  std::vector<AssetRecord> publishing(records.begin(), records.end());
  for (auto &record : publishing) {
    if ((!record.Package.empty() && record.Package != key) || record.OffsetBytes > payload.size() ||
        record.ByteCount > payload.size() - record.OffsetBytes) {
      return std::unexpected(AssetCacheError::InvalidInput);
    }
    record.Package = key;
    if (!AssetSql::Valid(record)) { return std::unexpected(AssetCacheError::InvalidInput); }
  }
  auto began = AssetSql::Exec(State_->Database, "BEGIN IMMEDIATE");
  if (!began) { return began; }
  Transaction transaction(State_->Database);
  const auto stored = StorePackage(State_->Database, key, payload);
  if (!stored) { return stored; }
  AssetSql::Statement asset;
  AssetSql::Statement bounds;
  auto prepared = AssetSql::Prepare(
      State_->Database,
      "INSERT INTO "
      "assets(key,kind,minx,miny,minz,maxx,maxy,maxz,package,offset,bytes,level,parent) "
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(key) DO UPDATE SET kind=excluded.kind,"
      "minx=excluded.minx,miny=excluded.miny,minz=excluded.minz,maxx=excluded.maxx,maxy=excluded."
      "maxy,"
      "maxz=excluded.maxz,package=excluded.package,offset=excluded.offset,bytes=excluded.bytes,"
      "level=excluded.level,parent=excluded.parent",
      asset);
  if (!prepared) { return prepared; }
  prepared =
      AssetSql::Prepare(State_->Database,
                        "INSERT OR REPLACE INTO bounds SELECT id,minx,maxx,miny,maxy,minz,maxz "
                        "FROM assets WHERE key=?",
                        bounds);
  if (!prepared) { return prepared; }
  for (const auto &record : publishing) {
    sqlite3_reset(asset.Value);
    sqlite3_reset(bounds.Value);
    if (!Bind(asset.Value, record) || !AssetSql::Text(bounds.Value, 1, record.Key)) {
      return std::unexpected(AssetCacheError::Storage);
    }
    int code = sqlite3_step(asset.Value);
    if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
    code = sqlite3_step(bounds.Value);
    if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
  }
  return transaction.Commit();
}

std::expected<void, AssetCacheError> AssetCache::Remove(std::string_view key) {
  if (!AssetSql::ValidKey(key)) { return std::unexpected(AssetCacheError::InvalidInput); }
  auto began = AssetSql::Exec(State_->Database, "BEGIN IMMEDIATE");
  if (!began) { return began; }
  Transaction transaction(State_->Database);
  for (const auto *sql : {"DELETE FROM bounds WHERE id IN (SELECT id FROM assets WHERE key=?)",
                          "DELETE FROM assets WHERE key=?"}) {
    AssetSql::Statement statement;
    auto prepared = AssetSql::Prepare(State_->Database, sql, statement);
    if (!prepared) { return prepared; }
    if (!AssetSql::Text(statement.Value, 1, key)) {
      return std::unexpected(AssetCacheError::Storage);
    }
    const int code = sqlite3_step(statement.Value);
    if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
  }
  return transaction.Commit();
}

}
