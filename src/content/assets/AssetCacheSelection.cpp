#include "AssetCacheState.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
namespace AssetSql {
namespace {

std::string Column(sqlite3_stmt *statement, int index) {
  const auto *text = sqlite3_column_text(statement, index);
  const int length = sqlite3_column_bytes(statement, index);
  return text == nullptr
             ? std::string{}
             : std::string(reinterpret_cast<const char *>(text), static_cast<size_t>(length));
}

}

std::optional<AssetRecord> Read(sqlite3_stmt *statement) {
  AssetRecord record;
  record.Key = Column(statement, 0);
  record.Kind = Column(statement, 1);
  for (int axis = 0; axis < 3; ++axis) {
    record.Bounds.Min[static_cast<size_t>(axis)] = sqlite3_column_double(statement, 2 + axis);
    record.Bounds.Max[static_cast<size_t>(axis)] = sqlite3_column_double(statement, 5 + axis);
  }
  const auto offset = sqlite3_column_int64(statement, 9);
  const auto bytes = sqlite3_column_int64(statement, 10);
  const auto level = sqlite3_column_int64(statement, 11);
  if (offset < 0 || bytes < 0 || level < 0 ||
      std::cmp_greater(level, std::numeric_limits<uint32_t>::max())) {
    return std::nullopt;
  }
  record.Package = Column(statement, 8);
  record.OffsetBytes = static_cast<uint64_t>(offset);
  record.ByteCount = static_cast<uint64_t>(bytes);
  record.Level = static_cast<uint32_t>(level);
  record.Parent = Column(statement, 12);
  if (sqlite3_column_count(statement) > 13) {
    const int type = sqlite3_column_type(statement, 16);
    if (type != SQLITE_NULL && type != SQLITE_FLOAT && type != SQLITE_INTEGER) {
      return std::nullopt;
    }
    record.Tile = AssetTile{.Revision = Column(statement, 13),
                            .Coverage = Column(statement, 14),
                            .Coarser = Column(statement, 15),
                            .ErrorM = type == SQLITE_NULL
                                          ? std::nullopt
                                          : std::optional(sqlite3_column_double(statement, 16))};
  }
  constexpr int kRequestColumn = 17;
  if (sqlite3_column_count(statement) > kRequestColumn) {
    record.RequestKey = Column(statement, kRequestColumn);
  }
  return Valid(record) ? std::optional(std::move(record)) : std::nullopt;
}

}

namespace {

bool Valid(const AssetQuery &query) {
  if (!AssetSql::Valid(query.Bounds) ||
      (query.Parent && !query.Parent->empty() && !AssetSql::ValidKey(*query.Parent)) ||
      (query.TileRevision && !AssetSql::ValidKey(*query.TileRevision)) ||
      (query.BoundTilesOnly && !query.TileRevision)) {
    return false;
  }
  if (query.Radius) {
    if (!std::isfinite(query.Radius->Metres) || query.Radius->Metres < 0 ||
        !std::ranges::all_of(
            query.Radius->Centre, [](double value) { return std::isfinite(value); })) {
      return false;
    }
  }
  return std::ranges::all_of(query.Planes, [](const auto &plane) {
    return std::ranges::all_of(plane, [](double value) { return std::isfinite(value); });
  });
}

bool Intersects(const Box &bounds, const AssetQuery &query) {
  Vec3 separation;
  for (size_t axis = 0; axis < 3; ++axis) {
    if (bounds.Min[axis] > query.Bounds.Max[axis] || bounds.Max[axis] < query.Bounds.Min[axis]) {
      return false;
    }
    if (query.Radius) {
      separation[axis] = std::max({bounds.Min[axis] - query.Radius->Centre[axis],
                                   query.Radius->Centre[axis] - bounds.Max[axis],
                                   0.0});
    }
  }
  if (query.Radius &&
      std::hypot(separation[0], separation[1], separation[2]) > query.Radius->Metres) {
    return false;
  }
  for (const auto &plane : query.Planes) {
    double support = plane[3];
    for (size_t axis = 0; axis < 3; ++axis) {
      support += plane[axis] * (plane[axis] >= 0 ? bounds.Max[axis] : bounds.Min[axis]);
    }
    if (support < 0) { return false; }
  }
  return true;
}

}

namespace {
std::expected<void, AssetCacheError> BindSelection(sqlite3_stmt *statement,
                                                   const AssetQuery &query) {
  for (int axis = 0; axis < 3; ++axis) {
    const auto index = static_cast<size_t>(axis);
    if (sqlite3_bind_double(statement, 1 + 2 * axis, query.Bounds.Max[index]) != SQLITE_OK ||
        sqlite3_bind_double(statement, 2 + 2 * axis, query.Bounds.Min[index]) != SQLITE_OK) {
      return std::unexpected(AssetCacheError::Storage);
    }
  }
  int parameter = 7;
  if (!query.Kind.empty()) {
    if (!AssetSql::Text(statement, parameter, query.Kind)) {
      return std::unexpected(AssetCacheError::Storage);
    }
    ++parameter;
  }
  if (query.Level) {
    if (sqlite3_bind_int64(statement, parameter, *query.Level) != SQLITE_OK) {
      return std::unexpected(AssetCacheError::Storage);
    }
    ++parameter;
  }
  if (query.Parent) {
    if (!AssetSql::Text(statement, parameter, *query.Parent)) {
      return std::unexpected(AssetCacheError::Storage);
    }
    ++parameter;
  }
  if (query.TileRevision && !AssetSql::Text(statement, parameter, *query.TileRevision)) {
    return std::unexpected(AssetCacheError::Storage);
  }
  return {};
}

enum class Lookup { Product, Request, Tile };

std::expected<std::optional<AssetRecord>, AssetCacheError>
FindRecord(sqlite3 *database, Lookup lookup, std::string_view key) {
  if (!AssetSql::ValidKey(key)) { return std::unexpected(AssetCacheError::InvalidInput); }
  AssetSql::Statement statement;
  std::string sql = std::string("SELECT ") + AssetSql::kColumns;
  if (lookup == Lookup::Request) {
    sql += " FROM asset_requests r JOIN assets a ON a.id=r.asset_id WHERE r.request_key=?";
  } else {
    if (lookup == Lookup::Tile) { sql += AssetSql::kTileColumns; }
    sql += " FROM assets a";
    if (lookup == Lookup::Tile) { sql += AssetSql::kTileJoin; }
    sql += " WHERE a.key=?";
  }
  auto prepared = AssetSql::Prepare(database, sql.c_str(), statement);
  if (!prepared) { return std::unexpected(prepared.error()); }
  if (!AssetSql::Text(statement.Value, 1, key)) {
    return std::unexpected(AssetCacheError::Storage);
  }
  const int code = sqlite3_step(statement.Value);
  if (code == SQLITE_DONE) { return std::optional<AssetRecord>{}; }
  if (code != SQLITE_ROW) { return std::unexpected(AssetSql::Error(code)); }
  auto record = AssetSql::Read(statement.Value);
  if (!record) { return std::unexpected(AssetCacheError::Storage); }
  if (lookup == Lookup::Request) { record->RequestKey = std::string(key); }
  return record;
}
}

std::expected<std::optional<AssetRecord>, AssetCacheError>
AssetCache::Find(std::string_view key) const {
  if (const auto opened = State_->EnsureOpen(); !opened) { return std::unexpected(opened.error()); }
  const std::scoped_lock lock(*State_->Mutex);
  return FindRecord(State_->Database, Lookup::Product, key);
}

std::expected<std::optional<AssetRecord>, AssetCacheError>
AssetCache::FindTile(std::string_view key) const {
  if (const auto opened = State_->EnsureOpen(); !opened) { return std::unexpected(opened.error()); }
  const std::scoped_lock lock(*State_->Mutex);
  return FindRecord(State_->Database, Lookup::Tile, key);
}

std::expected<std::vector<AssetRecord>, AssetCacheError>
AssetCache::FindTiles(std::span<const std::string_view> keys) const {
  if (keys.empty()) { return std::vector<AssetRecord>{}; }
  if (!std::ranges::all_of(keys, AssetSql::ValidKey)) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  if (const auto opened = State_->EnsureOpen(); !opened) { return std::unexpected(opened.error()); }
  const std::scoped_lock lock(*State_->Mutex);
  constexpr size_t kKeysPerBatch = 128;
  const size_t width = std::min(kKeysPerBatch, keys.size());
  std::string sql = std::string("SELECT ") + AssetSql::kColumns + AssetSql::kTileColumns +
                    " FROM assets a" + AssetSql::kTileJoin + " WHERE a.key IN (?";
  for (size_t at = 1; at < width; ++at) { sql += ",?"; }
  sql += ')';
  AssetSql::Statement statement;
  const auto prepared = AssetSql::Prepare(State_->Database, sql.c_str(), statement);
  if (!prepared) { return std::unexpected(prepared.error()); }
  std::vector<AssetRecord> records;
  records.reserve(keys.size());
  while (!keys.empty()) {
    const auto batch = keys.first(std::min(width, keys.size()));
    for (size_t at = 0; at < batch.size(); ++at) {
      if (!AssetSql::Text(statement.Value, static_cast<int>(at + 1), batch[at])) {
        return std::unexpected(AssetCacheError::Storage);
      }
    }
    int code = SQLITE_ROW;
    while ((code = sqlite3_step(statement.Value)) == SQLITE_ROW) {
      auto record = AssetSql::Read(statement.Value);
      if (!record) { return std::unexpected(AssetCacheError::Storage); }
      records.push_back(std::move(*record));
    }
    if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
    sqlite3_reset(statement.Value);
    sqlite3_clear_bindings(statement.Value);
    keys = keys.subspan(batch.size());
  }
  return records;
}

std::expected<std::optional<AssetRecord>, AssetCacheError>
AssetCache::FindRequest(std::string_view requestKey) const {
  if (const auto opened = State_->EnsureOpen(); !opened) { return std::unexpected(opened.error()); }
  const std::scoped_lock lock(*State_->Mutex);
  return FindRecord(State_->Database, Lookup::Request, requestKey);
}

std::expected<std::vector<AssetRecord>, AssetCacheError>
AssetCache::Select(const AssetQuery &query) const {
  if (const auto opened = State_->EnsureOpen(); !opened) { return std::unexpected(opened.error()); }
  const std::scoped_lock lock(*State_->Mutex);
  if (!Valid(query)) { return std::unexpected(AssetCacheError::InvalidInput); }
  AssetSql::Statement statement;
  std::string sql = std::string("SELECT ") + AssetSql::kColumns;
  if (query.TileRevision) { sql += AssetSql::kTileColumns; }
  if (query.BoundTilesOnly) { sql += ",q.request_key"; }
  sql += query.SpatialFirst ? " FROM bounds r CROSS JOIN assets a ON a.id=r.id"
                            : " FROM bounds r JOIN assets a ON a.id=r.id";
  if (query.TileRevision) {
    if (query.SpatialFirst) { sql += " CROSS"; }
    sql += AssetSql::kTileJoin;
  }
  if (query.BoundTilesOnly) { sql += " CROSS JOIN asset_requests q ON q.asset_id=a.id"; }
  sql += " WHERE r.minx<=? AND r.maxx>=? AND r.miny<=? AND r.maxy>=? AND r.minz<=? AND r.maxz>=?";
  if (!query.Kind.empty()) { sql += " AND a.kind=?"; }
  if (query.Level) { sql += " AND a.level=?"; }
  if (query.Parent) { sql += " AND a.parent=?"; }
  if (query.TileRevision) { sql += " AND t.revision=?"; }
  if (query.BoundParentOnly) {
    sql += " AND EXISTS(SELECT 1 FROM assets p JOIN asset_requests b ON b.asset_id=p.id "
           "WHERE p.key=a.parent)";
  }
  auto prepared = AssetSql::Prepare(State_->Database, sql.c_str(), statement);
  if (!prepared) { return std::unexpected(prepared.error()); }
  const auto bound = BindSelection(statement.Value, query);
  if (!bound) { return std::unexpected(bound.error()); }
  std::vector<AssetRecord> records;
  int code = SQLITE_ROW;
  while ((code = sqlite3_step(statement.Value)) == SQLITE_ROW) {
    auto record = AssetSql::Read(statement.Value);
    if (!record) { return std::unexpected(AssetCacheError::Storage); }
    if (Intersects(record->Bounds, query)) { records.push_back(std::move(*record)); }
  }
  if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
  return records;
}

std::expected<std::vector<AssetRecord>, AssetCacheError>
AssetCache::SelectTileChildren(const AssetTileChildren &query) const {
  if (!AssetSql::ValidKey(query.ParentRequest) || !AssetSql::ValidKey(query.Revision) ||
      query.Kind.empty() || query.Level == 0) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  if (const auto opened = State_->EnsureOpen(); !opened) { return std::unexpected(opened.error()); }
  const std::scoped_lock lock(*State_->Mutex);
  const std::string sql = std::string("SELECT ") + AssetSql::kColumns + AssetSql::kTileColumns +
                          ",q.request_key FROM assets a INDEXED BY assets_parent_kind_level" +
                          " CROSS" + AssetSql::kTileJoin +
                          " CROSS JOIN asset_requests q ON q.asset_id=a.id"
                          " WHERE a.parent=? AND a.kind=? AND a.level=? AND t.revision=?";
  AssetSql::Statement statement;
  const auto prepared = AssetSql::Prepare(State_->Database, sql.c_str(), statement);
  if (!prepared) { return std::unexpected(prepared.error()); }
  if (!AssetSql::Text(statement.Value, 1, query.ParentRequest) ||
      !AssetSql::Text(statement.Value, 2, query.Kind) ||
      sqlite3_bind_int64(statement.Value, 3, query.Level) != SQLITE_OK ||
      !AssetSql::Text(statement.Value, 4, query.Revision)) {
    return std::unexpected(AssetCacheError::Storage);
  }
  std::vector<AssetRecord> records;
  int code = SQLITE_ROW;
  while ((code = sqlite3_step(statement.Value)) == SQLITE_ROW) {
    auto record = AssetSql::Read(statement.Value);
    if (!record) { return std::unexpected(AssetCacheError::Storage); }
    records.push_back(std::move(*record));
  }
  if (code != SQLITE_DONE) { return std::unexpected(AssetSql::Error(code)); }
  return records;
}

}
