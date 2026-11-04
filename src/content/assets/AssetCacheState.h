#ifndef OUTSHINE_CONTENT_ASSETS_ASSETCACHESTATE_H
#define OUTSHINE_CONTENT_ASSETS_ASSETCACHESTATE_H

#include "AssetCache.h"
#include "CacheUsageIndex.h"
#include "SqliteFileBudget.h"
#include "JobAdmission.h"
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <sqlite3.h>

namespace outshine {

struct AssetCache::State {
  std::optional<JobAdmission::Lease> Connection;
  Runtime Execution;
  std::string Path;
  std::mutex Opening;
  std::atomic<bool> Opened{false};
  // Handles of one database share the lock; transaction helpers can call lookups.
  std::shared_ptr<std::recursive_mutex> Mutex;
  std::shared_ptr<SqliteFileBudget> Budget;
  sqlite3 *Database = nullptr;
  std::unique_ptr<CacheUsageIndex> Usage;
  bool UsageWarning = false;
  [[nodiscard]] std::expected<void, AssetCacheError> EnsureOpen();
  [[nodiscard]] std::expected<void, AssetCacheError> Initialize();
  void ReleaseConnection();
  void Observe(std::string_view key, uint64_t storedBytes, bool published = false);
  void WarnUsage();
  [[nodiscard]] std::expected<void, AssetCacheError> Reclaim(uint64_t required = 0) const;
  // Payload estimates guide reclamation; the VFS enforces the physical quota.
  [[nodiscard]] std::expected<void, AssetCacheError> BeginWrite(size_t payloadBytes = 0) const;
  [[nodiscard]] std::expected<void, AssetCacheError>
  StorePublication(const AssetCache::Publication &publication);
  ~State();
};

namespace AssetSql {

struct Statement {
  sqlite3_stmt *Value = nullptr;
  ~Statement();
  Statement() = default;
  Statement(const Statement &) = delete;
  Statement &operator=(const Statement &) = delete;
};

[[nodiscard]] AssetCacheError Error(int code);
[[nodiscard]] std::expected<void, AssetCacheError> Exec(sqlite3 *database, const char *sql);
[[nodiscard]] std::expected<void, AssetCacheError>
Prepare(sqlite3 *database, const char *sql, Statement &statement);
[[nodiscard]] bool Text(sqlite3_stmt *statement, int slot, std::string_view value);
[[nodiscard]] bool Valid(const Box &bounds);
[[nodiscard]] bool ValidKey(std::string_view key);
[[nodiscard]] bool Valid(const AssetRecord &record);
[[nodiscard]] std::optional<AssetRecord> Read(sqlite3_stmt *statement);

inline constexpr auto kColumns = "a.key,a.kind,a.minx,a.miny,a.minz,a.maxx,a.maxy,a.maxz,a.package,"
                                 "a.offset,a.bytes,a.level,a.parent";

}
}
#endif
