#ifndef OUTSHINE_CONTENT_ASSETS_ASSETINDEXSTATE_H
#define OUTSHINE_CONTENT_ASSETS_ASSETINDEXSTATE_H

#include "AssetIndex.h"
#include <sqlite3.h>

namespace outshine {

struct AssetIndex::State {
  sqlite3 *Database = nullptr;
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

[[nodiscard]] AssetIndexError Error(int code);
[[nodiscard]] std::expected<void, AssetIndexError> Exec(sqlite3 *database, const char *sql);
[[nodiscard]] std::expected<void, AssetIndexError>
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
