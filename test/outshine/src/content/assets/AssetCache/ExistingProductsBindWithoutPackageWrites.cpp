#include "Check.h"
#include "Sha256.h"
#include "AssetGeneration.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <sqlite3.h>
#include <string>

using namespace outshine;
using namespace outshine::Test;

namespace {
std::string Key(const std::string &value) {
  return Sha256Hex(value.data(), value.size());
}

void Exercise(const std::string &path) {
  auto opened = AssetCache::Open(path);
  CHECK(opened, "native store opens");
  if (!opened) { return; }
  auto &cache = **opened;
  const std::array<uint8_t, 4> payload{17, 23, 31, 47};
  const auto first = Key("first-product");
  const auto second = Key("second-product");
  const auto request = Key("generator/config/demand");
  const auto otherRequest = Key("generator/config/other-demand");
  const auto repairRequest = Key("generator/config/repair-demand");
  std::array<AssetRecord, 2> records{{{.Key = first,
                                       .Kind = "native",
                                       .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                       .Package = {},
                                       .ByteCount = payload.size(),
                                       .Parent = {}},
                                      {.Key = second,
                                       .Kind = "native",
                                       .Bounds = {.Min = {{2, 2, 2}}, .Max = {{3, 3, 3}}},
                                       .Package = {},
                                       .ByteCount = payload.size(),
                                       .Parent = {}}}};
  CHECK(cache.Publish(records, payload), "both products share one immutable package");
  sqlite3 *database = nullptr;
  CHECK(sqlite3_open(path.c_str(), &database) == SQLITE_OK, "failure injector opens the store");
  if (database == nullptr) { return; }
  CHECK(sqlite3_exec(database,
                     "CREATE TRIGGER forbid_package_update BEFORE UPDATE ON packages "
                     "BEGIN SELECT RAISE(ABORT,'package rewritten'); END; "
                     "CREATE TRIGGER forbid_package_insert BEFORE INSERT ON packages "
                     "BEGIN SELECT RAISE(ABORT,'package rewritten'); END;",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "test forbids all package writes during metadata binding");
  const auto missing = cache.BindRequest(Key("missing"), request);
  CHECK(missing && !*missing, "missing product cannot acquire a demand");
  CHECK(!cache.BindRequest(first, "bad"), "invalid demand fails before mutation");
  const auto bound = cache.BindRequest(first, request);
  CHECK(bound && *bound, "existing product binds without reading or rewriting its package");
  const auto replaced = cache.BindRequest(second, request);
  CHECK(replaced && *replaced, "another product atomically replaces the demand binding");
  auto found = cache.FindRequest(request);
  CHECK(found && *found && (**found).Key == second, "demand resolves the replacement");
  const auto preserved = cache.BindRequestIfUnbound(first, request);
  CHECK(preserved && !*preserved, "an older repair cannot replace a different current target");
  const auto repaired = cache.BindRequestIfUnbound(first, repairRequest);
  CHECK(repaired && *repaired, "an unbound request can acquire its existing native product");
  CHECK(!cache.BindRequestIfUnbound(first, "bad"), "repair rejects invalid request identities");
  const auto absent = cache.BindRequestIfUnbound(Key("missing"), Key("missing-repair"));
  CHECK(absent && !*absent, "repair does not bind a missing product");
  const std::string trigger =
      "CREATE TRIGGER reject_binding BEFORE INSERT ON asset_requests WHEN NEW.asset_id="
      "(SELECT id FROM assets WHERE key='" +
      first + "') BEGIN SELECT RAISE(ABORT,'binding refused'); END;";
  CHECK(sqlite3_exec(database, trigger.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
        "test rejects replacement at the normalized binding write");
  CHECK(!cache.BindRequest(first, request), "refused replacement returns an error");
  found = cache.FindRequest(request);
  CHECK(found && *found && (**found).Key == second, "rollback restores the previous binding");
  CHECK(sqlite3_exec(database, "DROP TRIGGER reject_binding", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "failure injector is removed");
  const auto renamed = cache.BindRequest(second, otherRequest);
  CHECK(renamed && *renamed, "product can retain another demand");
  found = cache.FindRequest(request);
  CHECK(found && *found && (**found).Key == second, "one product retains both demand identities");
  CHECK(sqlite3_exec(database, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "independent writer holds the mutation boundary");
  const auto unchanged = cache.BindRequest(second, otherRequest);
  CHECK(unchanged && *unchanged, "idempotent binding needs no write transaction under contention");
  const auto busy = cache.BindRequest(first, otherRequest);
  CHECK(!busy && busy.error() == AssetCacheError::Busy,
        "a real binding change still respects writer contention");
  const auto repairBusy = cache.BindRequestIfUnbound(first, Key("held-repair"));
  CHECK(!repairBusy && repairBusy.error() == AssetCacheError::Busy,
        "a missing binding reports real writer contention without losing its caller's work");
  const auto repairHit = cache.BindRequestIfUnbound(first, repairRequest);
  const auto repairConflict = cache.BindRequestIfUnbound(first, request);
  CHECK(repairHit && *repairHit && repairConflict && !*repairConflict,
        "existing equal and different bindings resolve without a writer transaction");
  CHECK(sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "independent writer releases its transaction");
  CHECK(sqlite3_exec(database,
                     "CREATE TRIGGER forbid_request_insert BEFORE INSERT ON asset_requests "
                     "BEGIN SELECT RAISE(ABORT,'request rewritten'); END;"
                     "CREATE TRIGGER forbid_request_update BEFORE UPDATE ON asset_requests "
                     "BEGIN SELECT RAISE(ABORT,'request rewritten'); END;",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "all request-index writes are forbidden during repeated hits");
  const auto repeated = cache.BindRequest(second, otherRequest);
  CHECK(repeated && *repeated, "repeated binding performs no request-index write");
  size_t generated = 0;
  auto loaded = ResolveAssetRequest(cache, otherRequest, 64, [&](size_t) {
    ++generated;
    return std::expected<GeneratedAssetPackage, std::string>(std::unexpected("unexpected miss"));
  });
  CHECK(loaded && generated == 0 && std::ranges::equal(loaded->Bytes(), payload),
        "native replay retains bytes and bypasses the producer");
  CHECK(sqlite3_close(database) == SQLITE_OK, "failure injector closes");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-asset-rebind-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  Exercise((root / "assets.sqlite").string());
  std::filesystem::remove_all(root);
  return Report();
}
