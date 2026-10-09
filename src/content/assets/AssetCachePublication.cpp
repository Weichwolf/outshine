#include "AssetCacheState.h"
#include "AssetPackageCodec.h"
#include "Sha256.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <string>
#include <vector>
#include <unordered_set>
#include <utility>
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
      AssetSql::Text(statement, 9, record.Package) &&
      AssetSql::Text(statement, 13, record.Parent) &&
      AssetSql::Text(statement,
                     14,
                     record.RequestKey ? std::string_view(*record.RequestKey) : std::string_view{});
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

std::expected<void, AssetCacheError> StorePackage(sqlite3 *database,
                                                  std::string_view key,
                                                  std::span<const uint8_t> stored,
                                                  int codec,
                                                  size_t nativeBytes) {
  AssetSql::Statement statement;
  const auto prepared =
      AssetSql::Prepare(database,
                        "INSERT INTO packages(key,bytes,crc,codec,native_bytes) VALUES(?,?,?,?,?) "
                        "ON CONFLICT(key) DO UPDATE SET bytes=excluded.bytes,crc=excluded.crc,"
                        "codec=excluded.codec,native_bytes=excluded.native_bytes",
                        statement);
  if (!prepared) { return prepared; }
  const auto checksum = crc32_z(0, stored.data(), stored.size());
  const int blob =
      stored.empty()
          ? sqlite3_bind_zeroblob(statement.Value, 2, 0)
          : sqlite3_bind_blob64(statement.Value, 2, stored.data(), stored.size(), SQLITE_STATIC);
  if (!AssetSql::Text(statement.Value, 1, key) || blob != SQLITE_OK ||
      sqlite3_bind_int64(statement.Value, 3, static_cast<sqlite3_int64>(checksum)) != SQLITE_OK ||
      sqlite3_bind_int(statement.Value, 4, codec) != SQLITE_OK ||
      sqlite3_bind_int64(statement.Value, 5, static_cast<sqlite3_int64>(nativeBytes)) !=
          SQLITE_OK) {
    return std::unexpected(AssetCacheError::Storage);
  }
  const int code = sqlite3_step(statement.Value);
  if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
  return {};
}

std::expected<void, AssetCacheError> RebindRequest(sqlite3_stmt *statement,
                                                   const AssetRecord &record) {
  if (!record.RequestKey) { return {}; }
  sqlite3_reset(statement);
  if (!AssetSql::Text(statement, 1, *record.RequestKey) ||
      !AssetSql::Text(statement, 2, record.Key)) {
    return std::unexpected(AssetCacheError::Storage);
  }
  const int code = sqlite3_step(statement);
  if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
  return {};
}

std::expected<std::vector<AssetRecord>, AssetCacheError> PrepareRecords(
    std::span<const AssetRecord> records, size_t payloadBytes, const std::string &package) {
  std::vector<AssetRecord> publishing(records.begin(), records.end());
  std::unordered_set<std::string_view> identities;
  std::unordered_set<std::string_view> requests;
  for (auto &record : publishing) {
    if ((!record.Package.empty() && record.Package != package) ||
        record.OffsetBytes > payloadBytes || record.ByteCount > payloadBytes - record.OffsetBytes) {
      return std::unexpected(AssetCacheError::InvalidInput);
    }
    record.Package = package;
    if (!AssetSql::Valid(record) || !identities.insert(record.Key).second ||
        (record.RequestKey && !requests.insert(*record.RequestKey).second)) {
      return std::unexpected(AssetCacheError::InvalidInput);
    }
  }
  return publishing;
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
  auto publishing = PrepareRecords(records, payload.size(), key);
  if (!publishing) { return std::unexpected(publishing.error()); }
  const auto compressed = AssetSql::Compress(payload);
  if (!compressed) { return std::unexpected(compressed.error()); }
  const int codec = compressed->empty() ? 0 : 1;
  const auto bytes = codec == 0 ? payload : std::span<const uint8_t>(*compressed);
  auto began = AssetSql::Exec(State_->Database, "BEGIN IMMEDIATE");
  if (!began) { return began; }
  Transaction transaction(State_->Database);
  const auto stored = StorePackage(State_->Database, key, bytes, codec, payload.size());
  if (!stored) { return stored; }
  AssetSql::Statement asset;
  AssetSql::Statement bounds;
  AssetSql::Statement request;
  auto prepared = AssetSql::Prepare(
      State_->Database,
      "INSERT INTO "
      "assets(key,kind,minx,miny,minz,maxx,maxy,maxz,package,offset,bytes,level,parent,request_key)"
      " "
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(key) DO UPDATE SET kind=excluded.kind,"
      "minx=excluded.minx,miny=excluded.miny,minz=excluded.minz,maxx=excluded.maxx,maxy=excluded."
      "maxy,"
      "maxz=excluded.maxz,package=excluded.package,offset=excluded.offset,bytes=excluded.bytes,"
      "level=excluded.level,parent=excluded.parent,request_key=excluded.request_key",
      asset);
  if (!prepared) { return prepared; }
  prepared =
      AssetSql::Prepare(State_->Database,
                        "INSERT OR REPLACE INTO bounds SELECT id,minx,maxx,miny,maxy,minz,maxz "
                        "FROM assets WHERE key=?",
                        bounds);
  if (!prepared) { return prepared; }
  prepared = AssetSql::Prepare(
      State_->Database, "UPDATE assets SET request_key='' WHERE request_key=? AND key<>?", request);
  if (!prepared) { return prepared; }
  for (const auto &record : *publishing) {
    const auto rebound = RebindRequest(request.Value, record);
    if (!rebound) { return rebound; }
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

std::expected<bool, AssetCacheError> AssetCache::BindRequest(std::string_view key,
                                                             std::string_view requestKey) {
  if (!AssetSql::ValidKey(key) || !AssetSql::ValidKey(requestKey)) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  auto began = AssetSql::Exec(State_->Database, "BEGIN IMMEDIATE");
  if (!began) { return std::unexpected(began.error()); }
  Transaction transaction(State_->Database);
  auto found = Find(key);
  if (!found) { return std::unexpected(found.error()); }
  if (!*found) { return false; }
  auto record = std::move(**found);
  record.RequestKey = std::string(requestKey);
  AssetSql::Statement previous;
  auto prepared =
      AssetSql::Prepare(State_->Database,
                        "UPDATE assets SET request_key='' WHERE request_key=? AND key<>?",
                        previous);
  if (!prepared) { return std::unexpected(prepared.error()); }
  auto rebound = RebindRequest(previous.Value, record);
  if (!rebound) { return std::unexpected(rebound.error()); }
  AssetSql::Statement target;
  prepared =
      AssetSql::Prepare(State_->Database, "UPDATE assets SET request_key=? WHERE key=?", target);
  if (!prepared) { return std::unexpected(prepared.error()); }
  rebound = RebindRequest(target.Value, record);
  if (!rebound) { return std::unexpected(rebound.error()); }
  auto committed = transaction.Commit();
  if (!committed) { return std::unexpected(committed.error()); }
  return true;
}

}
