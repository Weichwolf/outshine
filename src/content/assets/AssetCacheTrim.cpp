#include "CacheTrim.h"
#include "CacheTrimSql.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace outshine {
namespace {
namespace Sql = CacheTrimSql;

struct Store {
  std::filesystem::path Path;
  std::unique_ptr<Sql::Database> Database;
  bool Usage = false;
};

struct Package {
  std::string Key;
  uint64_t Bytes = 0;
  uint64_t Members = 0;
  int64_t Used = 0;
  int64_t Order = 0;
  size_t StoreIndex = 0;
};

std::expected<void, std::string>
Read(Store &store, size_t index, std::vector<Package> &packages, CacheTrimResult &result) {
  sqlite3 *const database = store.Database->Handle;
  Sql::Statement version;
  if (auto prepared = Sql::Prepare(database, "PRAGMA user_version", version); !prepared) {
    return prepared;
  }
  if (sqlite3_step(version.Handle) != SQLITE_ROW || sqlite3_column_int(version.Handle, 0) != 3) {
    return std::unexpected("unsupported native asset cache schema");
  }
  Sql::Statement schema;
  if (auto prepared = Sql::Prepare(database,
                                   "SELECT a.id,a.key,a.kind,a.minx,a.miny,a.minz,a.maxx,a.maxy,"
                                   "a.maxz,a.offset,a.bytes,a.level,a.parent,a.request_key,b.id,"
                                   "p.crc,p.codec,p.native_bytes FROM assets a "
                                   "LEFT JOIN bounds b ON b.id=a.id "
                                   "LEFT JOIN packages p ON p.key=a.package LIMIT 0",
                                   schema);
      !prepared) {
    return prepared;
  }
  Sql::Statement members;
  if (auto prepared = Sql::Prepare(database, "SELECT package FROM assets", members); !prepared) {
    return prepared;
  }
  std::unordered_map<std::string, uint64_t> counts;
  int step = SQLITE_OK;
  while ((step = sqlite3_step(members.Handle)) == SQLITE_ROW) {
    const auto *key = reinterpret_cast<const char *>(sqlite3_column_text(members.Handle, 0));
    if (key == nullptr || sqlite3_column_bytes(members.Handle, 0) != 64 || !Sql::Key(key)) {
      return std::unexpected("invalid asset package identity");
    }
    ++counts[key];
  }
  if (step != SQLITE_DONE) { return std::unexpected(sqlite3_errmsg(database)); }
  auto usage = Sql::Usage(database);
  if (!usage) { return std::unexpected(usage.error()); }
  auto hasUsage = Sql::HasUsage(database);
  if (!hasUsage) { return std::unexpected(hasUsage.error()); }
  store.Usage = *hasUsage;
  Sql::Statement query;
  if (auto prepared = Sql::Prepare(
          database, "SELECT key,length(bytes),rowid,typeof(bytes) FROM packages", query);
      !prepared) {
    return prepared;
  }
  while ((step = sqlite3_step(query.Handle)) == SQLITE_ROW) {
    const auto *key = reinterpret_cast<const char *>(sqlite3_column_text(query.Handle, 0));
    const auto *type = reinterpret_cast<const char *>(sqlite3_column_text(query.Handle, 3));
    const auto bytes = sqlite3_column_int64(query.Handle, 1);
    if (key == nullptr || sqlite3_column_bytes(query.Handle, 0) != 64 || !Sql::Key(key) ||
        bytes < 0 || type == nullptr || std::string_view(type) != "blob" ||
        static_cast<uint64_t>(bytes) > std::numeric_limits<uint64_t>::max() - result.StoredBytes) {
      return std::unexpected("invalid asset package metadata");
    }
    const auto observed = usage->find(key);
    const bool unknown = observed == usage->end();
    result.UnknownUse += unknown ? 1u : 0u;
    result.StoredBytes += static_cast<uint64_t>(bytes);
    packages.push_back({.Key = key,
                        .Bytes = static_cast<uint64_t>(bytes),
                        .Members = counts[key],
                        .Used = unknown ? 0 : observed->second,
                        .Order = sqlite3_column_int64(query.Handle, 2),
                        .StoreIndex = index});
  }
  if (step != SQLITE_DONE) { return std::unexpected(sqlite3_errmsg(database)); }
  return {};
}

std::expected<void, std::string>
Erase(Store &store, std::span<const Package> packages, size_t index) {
  sqlite3 *const database = store.Database->Handle;
  if (auto created = Sql::Exec(database, "CREATE TEMP TABLE trim_packages(key TEXT PRIMARY KEY)");
      !created) {
    return created;
  }
  Sql::Statement insert;
  if (auto prepared = Sql::Prepare(database, "INSERT INTO trim_packages VALUES(?)", insert);
      !prepared) {
    return prepared;
  }
  for (const auto &package : packages) {
    if (package.StoreIndex != index) { continue; }
    if (sqlite3_bind_text(insert.Handle, 1, package.Key.c_str(), -1, SQLITE_TRANSIENT) !=
            SQLITE_OK ||
        sqlite3_step(insert.Handle) != SQLITE_DONE || sqlite3_reset(insert.Handle) != SQLITE_OK) {
      return std::unexpected(sqlite3_errmsg(database));
    }
  }
  for (const auto *sql :
       {"DELETE FROM bounds WHERE id IN (SELECT id FROM assets WHERE package IN trim_packages)",
        "DELETE FROM assets WHERE package IN trim_packages",
        "DELETE FROM packages WHERE key IN trim_packages"}) {
    if (auto erased = Sql::Exec(database, sql); !erased) { return erased; }
  }
  if (store.Usage) {
    if (auto erased = Sql::Exec(database,
                                "DELETE FROM cache_usage_v1 WHERE key NOT IN "
                                "(SELECT key FROM packages)");
        !erased) {
      return erased;
    }
  }
  return {};
}

void Choose(std::vector<Package> &packages, CacheTrimOptions options, CacheTrimResult &result) {
  std::ranges::sort(packages, [](const Package &left, const Package &right) {
    if ((left.Members == 0) != (right.Members == 0)) { return left.Members == 0; }
    if (left.Used != right.Used) { return left.Used < right.Used; }
    if (left.StoreIndex != right.StoreIndex) { return left.StoreIndex < right.StoreIndex; }
    return left.Order < right.Order;
  });
  result.RetainedBytes = result.StoredBytes;
  size_t selected = 0;
  for (const auto &package : packages) {
    if (options.BudgetBytes != 0 && package.Members != 0 &&
        result.RetainedBytes <= options.BudgetBytes) {
      break;
    }
    result.RetainedBytes -= package.Bytes;
    result.EntriesRemoved += package.Members;
    ++result.PackagesRemoved;
    ++selected;
  }
  packages.resize(selected);
}
}

std::expected<CacheTrimResult, std::string>
TrimAssetCaches(std::span<const std::filesystem::path> paths, CacheTrimOptions options) {
  CacheTrimResult result;
  std::vector<Store> stores;
  std::vector<Package> packages;
  for (const auto &path : paths) {
    auto exists = Sql::Exists(path);
    if (!exists) { return std::unexpected(exists.error()); }
    if (!*exists) { continue; }
    auto fileBytes = Sql::FileBytes(path);
    if (!fileBytes) { return std::unexpected(fileBytes.error()); }
    result.FileBytesBefore += *fileBytes;
    auto database = Sql::Open(path, options.Apply);
    if (!database) { return std::unexpected(database.error()); }
    stores.push_back({.Path = path, .Database = std::move(*database)});
    if (auto read = Read(stores.back(), stores.size() - 1, packages, result); !read) {
      return std::unexpected(path.string() + ": " + read.error());
    }
  }
  Choose(packages, options, result);
  result.FileBytesAfter = result.FileBytesBefore;
  if (!options.Apply) { return result; }
  for (size_t i = 0; i < stores.size(); ++i) {
    if (auto erased = Erase(stores[i], packages, i); !erased) {
      return std::unexpected(stores[i].Path.string() + ": " + erased.error());
    }
  }
  result.FileBytesAfter = 0;
  for (auto &store : stores) {
    if (auto committed = Sql::Commit(*store.Database, options.Compact); !committed) {
      return std::unexpected(store.Path.string() + ": " + committed.error());
    }
    store.Database.reset();
    auto bytes = Sql::FileBytes(store.Path);
    if (!bytes) { return std::unexpected(bytes.error()); }
    result.FileBytesAfter += *bytes;
  }
  return result;
}

}
