#include "AssetCache.h"
#include "AssetCacheTrim.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <sqlite3.h>
#include <string>
#include <system_error>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

Database Connect(const std::string &path) {
  sqlite3 *raw = nullptr;
  const int opened = sqlite3_open(path.c_str(), &raw);
  Database database(raw, &sqlite3_close);
  CHECK(opened == SQLITE_OK, "the migration opponent owns an independent SQL connection");
  if (opened != SQLITE_OK) { database.reset(); }
  return database;
}

bool Execute(sqlite3 *database, const char *sql) {
  return sqlite3_exec(database, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

std::optional<int> Scalar(sqlite3 *database, const char *sql) {
  sqlite3_stmt *raw = nullptr;
  if (sqlite3_prepare_v2(database, sql, -1, &raw, nullptr) != SQLITE_OK) { return std::nullopt; }
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw,
                                                                             &sqlite3_finalize);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) { return std::nullopt; }
  return sqlite3_column_int(statement.get(), 0);
}

void Migration(const std::string &path) {
  const std::array<uint8_t, 8> bytes{1, 3, 5, 7, 9, 11, 13, 15};
  const std::string key(64, 'a');
  const std::string request(64, 'b');
  const AssetRecord record{.Key = key,
                           .Kind = "inline-migration",
                           .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                           .Package = {},
                           .ByteCount = bytes.size(),
                           .Parent = {},
                           .RequestKey = request};
  auto store = AssetCache::Open(path);
  CHECK(store && (*store)->Publish(std::span(&record, 1), bytes),
        "a real product and request binding supply the old-schema input");
  if (!store) { return; }
  store->reset();
  const auto database = Connect(path);
  if (!database) { return; }
  CHECK(
      Execute(database.get(),
              "UPDATE packages SET bytes=(SELECT bytes FROM package_bodies WHERE key=packages.key);"
              "DROP TABLE package_bodies;PRAGMA user_version=4;"
              "CREATE TRIGGER forbid_insert BEFORE INSERT ON packages BEGIN SELECT "
              "RAISE(ABORT,'insert'); END;"
              "CREATE TRIGGER forbid_update BEFORE UPDATE ON packages BEGIN SELECT "
              "RAISE(ABORT,'update'); END;"
              "CREATE TRIGGER forbid_delete BEFORE DELETE ON packages BEGIN SELECT "
              "RAISE(ABORT,'delete'); END;"),
      "the exact version-four inline fixture refuses every package payload write");
  const std::array paths{std::filesystem::path(path)};
  const auto legacyTrim = TrimAssetCaches(paths, {.BudgetBytes = bytes.size()});
  CHECK(legacyTrim && legacyTrim->StoredBytes == bytes.size() &&
            legacyTrim->RetainedBytes == bytes.size(),
        "cache accounting also reads the original inline schema without changing it");
  store = AssetCache::Open(path);
  CHECK(store && Scalar(database.get(), "PRAGMA user_version") == 5 &&
            Scalar(database.get(), "SELECT count(*) FROM package_bodies") == 0 &&
            Scalar(database.get(), "SELECT sum(length(bytes)) FROM packages") == 8,
        "opening adds only streamed storage metadata while preserving every old payload byte");
  if (!store) { return; }
  const auto mixedTrim = TrimAssetCaches(paths, {.BudgetBytes = bytes.size()});
  CHECK(mixedTrim && mixedTrim->StoredBytes == bytes.size(),
        "the current schema still counts preserved inline bodies exactly once");
  const auto old = (*store)->LoadRequest(request, bytes.size());
  const auto plan = (*store)->PlanRead(key, bytes.size());
  CHECK(old && *old && std::ranges::equal((**old).Bytes(), bytes) && plan && *plan &&
            (**plan).PackageBytesMost == bytes.size() && (**plan).BufferBytesMost == bytes.size(),
        "ordinary bounded loading and planning consume the untouched inline package offline");
  CHECK(Execute(database.get(), "BEGIN IMMEDIATE"), "an independent writer holds the new schema");
  auto reader = AssetCache::Open(path);
  CHECK(reader, "a current-schema reader opens without acquiring another migration writer lock");
  CHECK(Execute(database.get(),
                "ROLLBACK;DROP TRIGGER forbid_insert;DROP TRIGGER forbid_update;"
                "DROP TRIGGER forbid_delete;"),
        "the fixture removes only its writer and payload-write opponents");
  CHECK((*store)->Publish(std::span(&record, 1), bytes),
        "an explicit publication may replace the old inline representation");
  const auto current = (*store)->LoadRequest(request, bytes.size());
  CHECK(current && *current && old && *old &&
            (**current).Record().Package == (**old).Record().Package &&
            std::ranges::equal((**current).Bytes(), bytes) &&
            Scalar(database.get(), "SELECT sum(length(bytes)) FROM packages") == 0 &&
            Scalar(database.get(), "SELECT sum(length(bytes)) FROM package_bodies") == 8,
        "explicit republication keeps native identity and binding with exactly one retained body");
  const auto streamedTrim = TrimAssetCaches(paths, {.BudgetBytes = bytes.size()});
  CHECK(streamedTrim && streamedTrim->StoredBytes == bytes.size(),
        "cache accounting counts the replacement streamed body exactly once");
}
}

int main() {
  auto root =
      (std::filesystem::temp_directory_path() / "outshine-inline-migration-XXXXXX").string();
  CHECK(mkdtemp(root.data()) != nullptr, "the legacy opponent owns an isolated directory");
  if (!std::filesystem::is_directory(root)) { return Report(); }
  Migration(root + "/assets.sqlite");
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the migration fixture closes its connections before cleanup");
  return Report();
}
