#include "AssetIndexState.h"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <span>
#include <string_view>

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

  [[nodiscard]] std::expected<void, AssetIndexError> Commit() {
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

}

std::expected<void, AssetIndexError> AssetIndex::Publish(std::span<const AssetRecord> records) {
  if (!std::ranges::all_of(records,
                           [](const AssetRecord &record) { return AssetSql::Valid(record); })) {
    return std::unexpected(AssetIndexError::InvalidInput);
  }
  if (records.empty()) { return {}; }
  auto began = AssetSql::Exec(State_->Database, "BEGIN IMMEDIATE");
  if (!began) { return began; }
  Transaction transaction(State_->Database);
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
  for (const auto &record : records) {
    sqlite3_reset(asset.Value);
    sqlite3_reset(bounds.Value);
    if (!Bind(asset.Value, record) || !AssetSql::Text(bounds.Value, 1, record.Key)) {
      return std::unexpected(AssetIndexError::Storage);
    }
    int code = sqlite3_step(asset.Value);
    if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
    code = sqlite3_step(bounds.Value);
    if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
  }
  return transaction.Commit();
}

std::expected<void, AssetIndexError> AssetIndex::Remove(std::string_view key) {
  if (!AssetSql::ValidKey(key)) { return std::unexpected(AssetIndexError::InvalidInput); }
  auto began = AssetSql::Exec(State_->Database, "BEGIN IMMEDIATE");
  if (!began) { return began; }
  Transaction transaction(State_->Database);
  for (const auto *sql : {"DELETE FROM bounds WHERE id IN (SELECT id FROM assets WHERE key=?)",
                          "DELETE FROM assets WHERE key=?"}) {
    AssetSql::Statement statement;
    auto prepared = AssetSql::Prepare(State_->Database, sql, statement);
    if (!prepared) { return prepared; }
    if (!AssetSql::Text(statement.Value, 1, key)) {
      return std::unexpected(AssetIndexError::Storage);
    }
    const int code = sqlite3_step(statement.Value);
    if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
  }
  return transaction.Commit();
}

}
