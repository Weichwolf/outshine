#include "AssetCacheState.h"
#include "AssetPackageCodec.h"
#include "Sha256.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <string>
#include <vector>
#include <unordered_set>
#include <utility>
#include <zlib.h>

namespace outshine {
size_t PreparedAssetWrite::CapacityBytes() const noexcept {
  size_t bytes = sizeof(*this);
  const auto add = [&bytes](size_t count) {
    bytes = count > std::numeric_limits<size_t>::max() - bytes ? std::numeric_limits<size_t>::max()
                                                               : bytes + count;
  };
  const auto text = [&add](const std::string &value) {
    add(value.capacity());
    add(1);
  };
  add(Bytes_.capacity());
  add(Records_.capacity() * sizeof(AssetRecord));
  text(Key_);
  for (const auto &record : Records_) {
    text(record.Key);
    text(record.Kind);
    text(record.Package);
    text(record.Parent);
    if (record.RequestKey) { text(*record.RequestKey); }
  }
  return bytes;
}

struct AssetCache::Publication {
  std::span<const AssetRecord> Records;
  std::span<const uint8_t> Bytes;
  std::string_view Key;
  size_t NativeBytes = 0;
  bool Compressed = false;
};

namespace {

constexpr auto kBindRequest =
    "INSERT INTO asset_requests(request_key,asset_id) SELECT ?,id FROM assets WHERE key=? "
    "ON CONFLICT(request_key) DO UPDATE SET asset_id=excluded.asset_id "
    "WHERE asset_requests.asset_id<>excluded.asset_id";

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

std::expected<void, AssetCacheError> Erase(sqlite3 *database, std::string_view key) {
  for (const auto *sql :
       {"DELETE FROM asset_requests WHERE asset_id IN (SELECT id FROM assets WHERE key=?)",
        "DELETE FROM bounds WHERE id IN (SELECT id FROM assets WHERE key=?)",
        "DELETE FROM assets WHERE key=?"}) {
    AssetSql::Statement statement;
    auto prepared = AssetSql::Prepare(database, sql, statement);
    if (!prepared) { return prepared; }
    if (!AssetSql::Text(statement.Value, 1, key)) {
      return std::unexpected(AssetCacheError::Storage);
    }
    const int code = sqlite3_step(statement.Value);
    if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
  }
  return {};
}

std::expected<bool, AssetCacheError> Validate(const std::optional<CachedAsset> &asset,
                                              std::string_view kind,
                                              const AssetCache::Validator &validate) {
  if (!asset) { return false; }
  if (asset->Record().Kind != kind) { return std::unexpected(AssetCacheError::InvalidInput); }
  return validate(*asset);
}

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
    std::vector<AssetRecord> publishing, size_t payloadBytes, const std::string &package) {
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
  std::unique_lock lock(*State_->Mutex);
  if (std::cmp_greater(payload.size(), sqlite3_limit(State_->Database, SQLITE_LIMIT_LENGTH, -1))) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  lock.unlock();
  const std::string key = Sha256Hex(payload.data(), payload.size());
  auto publishing =
      PrepareRecords(std::vector<AssetRecord>(records.begin(), records.end()), payload.size(), key);
  if (!publishing) { return std::unexpected(publishing.error()); }
  const auto compressed = AssetSql::Compress(payload);
  if (!compressed) { return std::unexpected(compressed.error()); }
  const bool encoded = !compressed->empty();
  const auto bytes = encoded ? std::span<const uint8_t>(*compressed) : payload;
  return PublishPrepared({.Records = *publishing,
                          .Bytes = bytes,
                          .Key = key,
                          .NativeBytes = payload.size(),
                          .Compressed = encoded});
}

std::expected<PreparedAssetWrite, AssetCacheError>
AssetCache::PreparePublication(std::vector<AssetRecord> records, std::vector<uint8_t> payload) {
  if (records.empty()) { return std::unexpected(AssetCacheError::InvalidInput); }
  {
    const std::scoped_lock lock(*State_->Mutex);
    if (std::cmp_greater(payload.size(),
                         sqlite3_limit(State_->Database, SQLITE_LIMIT_LENGTH, -1))) {
      return std::unexpected(AssetCacheError::CapacityExceeded);
    }
  }
  auto key = Sha256Hex(payload.data(), payload.size());
  auto publishing = PrepareRecords(std::move(records), payload.size(), key);
  if (!publishing) { return std::unexpected(publishing.error()); }
  auto compressed = AssetSql::Compress(payload);
  if (!compressed) { return std::unexpected(compressed.error()); }
  PreparedAssetWrite prepared;
  prepared.NativeBytes_ = payload.size();
  prepared.Compressed_ = !compressed->empty();
  prepared.Records_ = std::move(*publishing);
  prepared.Key_ = std::move(key);
  prepared.Bytes_ = prepared.Compressed_ ? std::move(*compressed) : std::move(payload);
  return prepared;
}

std::expected<size_t, AssetCacheError> AssetCache::PublicationBufferBytes(size_t nativeBytes) {
  return AssetSql::CompressionBytes(nativeBytes);
}

std::expected<void, AssetCacheError> AssetCache::Publish(const PreparedAssetWrite &prepared) {
  if (prepared.Records_.empty() || prepared.Key_.empty()) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  return PublishPrepared({.Records = prepared.Records_,
                          .Bytes = prepared.Bytes_,
                          .Key = prepared.Key_,
                          .NativeBytes = prepared.NativeBytes_,
                          .Compressed = prepared.Compressed_});
}

std::expected<void, AssetCacheError> AssetCache::PublishPrepared(const Publication &publication) {
  const std::scoped_lock lock(*State_->Mutex);
  if (std::cmp_greater(publication.NativeBytes,
                       sqlite3_limit(State_->Database, SQLITE_LIMIT_LENGTH, -1))) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  auto began = AssetSql::Exec(State_->Database, "BEGIN IMMEDIATE");
  if (!began) { return began; }
  Transaction transaction(State_->Database);
  const auto stored = StorePackage(State_->Database,
                                   publication.Key,
                                   publication.Bytes,
                                   publication.Compressed ? 1 : 0,
                                   publication.NativeBytes);
  if (!stored) { return stored; }
  AssetSql::Statement asset;
  AssetSql::Statement bounds;
  AssetSql::Statement request;
  auto prepared = AssetSql::Prepare(
      State_->Database,
      "INSERT INTO "
      "assets(key,kind,minx,miny,minz,maxx,maxy,maxz,package,offset,bytes,level,parent)"
      " "
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
  prepared = AssetSql::Prepare(State_->Database, kBindRequest, request);
  if (!prepared) { return prepared; }
  for (const auto &record : publication.Records) {
    sqlite3_reset(asset.Value);
    sqlite3_reset(bounds.Value);
    if (!Bind(asset.Value, record) || !AssetSql::Text(bounds.Value, 1, record.Key)) {
      return std::unexpected(AssetCacheError::Storage);
    }
    int code = sqlite3_step(asset.Value);
    if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
    code = sqlite3_step(bounds.Value);
    if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
    const auto rebound = RebindRequest(request.Value, record);
    if (!rebound) { return rebound; }
  }
  auto committed = transaction.Commit();
  if (committed) { State_->Observe(publication.Key, publication.Bytes.size(), true); }
  return committed;
}

std::expected<void, AssetCacheError> AssetCache::Remove(std::string_view key) {
  const std::scoped_lock lock(*State_->Mutex);
  if (!AssetSql::ValidKey(key)) { return std::unexpected(AssetCacheError::InvalidInput); }
  auto began = AssetSql::Exec(State_->Database, "BEGIN IMMEDIATE");
  if (!began) { return began; }
  Transaction transaction(State_->Database);
  const auto erased = Erase(State_->Database, key);
  if (!erased) { return erased; }
  return transaction.Commit();
}

std::expected<std::optional<CachedAsset>, AssetCacheError>
AssetCache::LoadValidated(std::string_view key,
                          std::string_view kind,
                          size_t packageBytesMost,
                          const Validator &validate) {
  if (!AssetSql::ValidKey(key) || kind.empty() || packageBytesMost == 0 || !validate) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  auto loaded = Load(key, packageBytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  auto valid = Validate(*loaded, kind, validate);
  if (!valid) { return std::unexpected(valid.error()); }
  if (*valid) { return std::move(*loaded); }
  const std::scoped_lock lock(*State_->Mutex);
  const auto found = Find(key);
  if (!found) { return std::unexpected(found.error()); }
  if (!*found) { return std::nullopt; }
  if ((**found).Kind != kind) { return std::unexpected(AssetCacheError::InvalidInput); }
  const auto began = AssetSql::Exec(State_->Database, "BEGIN IMMEDIATE");
  if (!began) { return std::unexpected(began.error()); }
  Transaction transaction(State_->Database);
  loaded = Load(key, packageBytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  valid = Validate(*loaded, kind, validate);
  if (!valid) { return std::unexpected(valid.error()); }
  if (*valid) { return std::move(*loaded); }
  if (!*loaded) {
    const auto current = Find(key);
    if (!current) { return std::unexpected(current.error()); }
    if (*current && (**current).Kind != kind) {
      return std::unexpected(AssetCacheError::InvalidInput);
    }
  }
  const auto erased = Erase(State_->Database, key);
  if (!erased) { return std::unexpected(erased.error()); }
  const auto committed = transaction.Commit();
  if (!committed) { return std::unexpected(committed.error()); }
  return std::nullopt;
}

std::expected<bool, AssetCacheError> AssetCache::BindRequest(std::string_view key,
                                                             std::string_view requestKey) {
  return BindRequest(key, requestKey, true);
}

std::expected<bool, AssetCacheError> AssetCache::BindRequestIfUnbound(std::string_view key,
                                                                      std::string_view requestKey) {
  return BindRequest(key, requestKey, false);
}

std::expected<bool, AssetCacheError> AssetCache::BindRequestIfMatches(std::string_view key,
                                                                      const AssetRecord &binding) {
  if (!binding.RequestKey || !AssetSql::ValidKey(*binding.RequestKey) ||
      !AssetSql::Valid(binding)) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  return BindRequest(key, *binding.RequestKey, true, &binding);
}

std::expected<bool, AssetCacheError>
AssetCache::UnbindRequestIfMatches(const AssetRecord &binding) {
  const auto &request = binding.RequestKey;
  if (!request || !AssetSql::ValidKey(*request) || !AssetSql::Valid(binding)) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  const auto matches = [&](const auto &found) { return found && *found == binding; };
  const auto guard = std::scoped_lock(*State_->Mutex);
  const auto previous = FindRequest(*request);
  if (!previous) { return std::unexpected(previous.error()); }
  if (!matches(*previous)) { return false; }
  const auto began = AssetSql::Exec(State_->Database, "BEGIN IMMEDIATE");
  if (!began) { return std::unexpected(began.error()); }
  Transaction transaction(State_->Database);
  const auto current = FindRequest(*request);
  if (!current) { return std::unexpected(current.error()); }
  if (!matches(*current)) { return false; }
  AssetSql::Statement erase;
  const auto prepared =
      AssetSql::Prepare(State_->Database, "DELETE FROM asset_requests WHERE request_key=?", erase);
  if (!prepared) { return std::unexpected(prepared.error()); }
  if (!AssetSql::Text(erase.Value, 1, *request)) {
    return std::unexpected(AssetCacheError::Storage);
  }
  const int code = sqlite3_step(erase.Value);
  if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
  const auto committed = transaction.Commit();
  if (!committed) { return std::unexpected(committed.error()); }
  return true;
}

std::expected<bool, AssetCacheError> AssetCache::BindRequest(std::string_view key,
                                                             std::string_view requestKey,
                                                             bool replace,
                                                             const AssetRecord *binding) {
  const std::scoped_lock lock(*State_->Mutex);
  if (!AssetSql::ValidKey(key) || !AssetSql::ValidKey(requestKey)) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  const auto allowed = [&](const std::optional<AssetRecord> &current) {
    return binding != nullptr ? current && *current == *binding : replace || !current;
  };
  const auto previous = FindRequest(requestKey);
  if (!previous) { return std::unexpected(previous.error()); }
  if (*previous && (**previous).Key == key) { return true; }
  if (!allowed(*previous)) { return false; }
  auto began = AssetSql::Exec(State_->Database, "BEGIN IMMEDIATE");
  if (!began) { return std::unexpected(began.error()); }
  Transaction transaction(State_->Database);
  if (!replace || binding != nullptr) {
    const auto current = FindRequest(requestKey);
    if (!current) { return std::unexpected(current.error()); }
    if (*current && (**current).Key == key) { return true; }
    if (!allowed(*current)) { return false; }
  }
  auto found = Find(key);
  if (!found) { return std::unexpected(found.error()); }
  if (!*found) { return false; }
  auto record = std::move(**found);
  record.RequestKey = std::string(requestKey);
  AssetSql::Statement target;
  auto prepared = AssetSql::Prepare(State_->Database, kBindRequest, target);
  if (!prepared) { return std::unexpected(prepared.error()); }
  const auto rebound = RebindRequest(target.Value, record);
  if (!rebound) { return std::unexpected(rebound.error()); }
  auto committed = transaction.Commit();
  if (!committed) { return std::unexpected(committed.error()); }
  return true;
}

}
