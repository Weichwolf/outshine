#include "AssetCache.h"
#include "Check.h"
#include "Sha256.h"
#include "SqliteFileBudget.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <dlfcn.h>
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kMiB = size_t{1024} * 1024;
constexpr uint64_t kLimit = 12 * kMiB;
std::atomic_bool ObserveBodies{false};
std::atomic_size_t BodyStatements{0};

int Trace(unsigned event, void * /*context*/, void *statement, void * /*detail*/) {
  if (event == SQLITE_TRACE_STMT && ObserveBodies.load(std::memory_order_relaxed)) {
    const auto *sql = sqlite3_sql(static_cast<sqlite3_stmt *>(statement));
    if (sql != nullptr && std::string_view(sql).starts_with("INSERT INTO package_bodies")) {
      BodyStatements.fetch_add(1, std::memory_order_relaxed);
    }
  }
  return 0;
}

struct Environment {
  std::filesystem::path Root =
      std::filesystem::temp_directory_path() /
      ("outshine-large-publication-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  Environment() {
    std::error_code error;
    std::filesystem::create_directories(Root, error);
    CHECK(!error, "an isolated cache root is created");
  }

  ~Environment() {
    std::error_code error;
    std::filesystem::remove_all(Root, error);
  }
};

struct Package {
  uint64_t Id;
  size_t Bytes;
};

auto Prepare(AssetCache &cache, Package package) {
  std::vector<uint8_t> payload(package.Bytes);
  uint64_t state = package.Id + 1;
  for (auto &byte : payload) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    byte = static_cast<uint8_t>(state);
  }
  const AssetRecord record{.Key = Sha256Hex(&package.Id, sizeof(package.Id)),
                           .Kind = "quota-test",
                           .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                           .Package = {},
                           .ByteCount = payload.size(),
                           .Parent = {}};
  return cache.PreparePublication({record}, std::move(payload));
}

bool Publish(AssetCache &cache, const PreparedAssetWrite &prepared) {
  for (unsigned attempt = 0; attempt < 32; ++attempt) {
    const auto published = cache.Publish(prepared);
    if (published) { return true; }
    if (published.error() != AssetCacheError::Busy) { return false; }
  }
  return false;
}

void NearQuotaEstimate() {
  const Environment environment;
  CHECK(SqliteFileBudget::Configure(environment.Root, kLimit),
        "a fresh physical quota is configured");
  const auto cache = AssetCache::Open((environment.Root / "assets.sqlite").string());
  const auto budget = SqliteFileBudget::Open(environment.Root);
  CHECK(cache && budget, "the nearly full package has an empty bank");
  if (!cache || !budget) { return; }
  const auto bytes = (kLimit - (*budget)->Reserve()) / 2 - uint64_t{160} * 1024;
  const auto prepared = Prepare(**cache, {.Id = 100, .Bytes = static_cast<size_t>(bytes)});
  CHECK(prepared && Publish(**cache, *prepared),
        "a conservative estimate cannot reject a package that physically fits");
  const auto usage = (*budget)->Inspect();
  CHECK(usage && usage->ChargedBytes <= kLimit, "near-capacity admission preserves the quota");
}
}

extern "C" int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs) {
  using Open = int (*)(const char *, sqlite3 **, int, const char *);
  static const auto open = reinterpret_cast<Open>(dlsym(RTLD_NEXT, "sqlite3_open_v2"));
  if (open == nullptr) { return SQLITE_CANTOPEN; }
  const int status = open(filename, ppDb, flags, zVfs);
  if (status == SQLITE_OK) { (void)sqlite3_trace_v2(*ppDb, SQLITE_TRACE_STMT, Trace, nullptr); }
  return status;
}

int main() {
  const Environment environment;
  CHECK(SqliteFileBudget::Configure(environment.Root, kLimit), "the physical quota is configured");
  const auto cache = AssetCache::Open((environment.Root / "assets.sqlite").string());
  CHECK(cache.has_value(), "the cache opens");
  if (!cache) { return Report(); }
  for (uint64_t id = 0; id < 3; ++id) {
    const auto prepared = Prepare(**cache, {.Id = id, .Bytes = 2 * kMiB});
    CHECK(prepared && Publish(**cache, *prepared), "incompressible existing assets fill the bank");
  }
  const auto budget = SqliteFileBudget::Open(environment.Root);
  CHECK(budget.has_value(), "the shared quota can be inspected");
  if (!budget) { return Report(); }
  sqlite3 *raw = nullptr;
  CHECK(sqlite3_open_v2((environment.Root / "assets.sqlite").c_str(),
                        &raw,
                        SQLITE_OPEN_READWRITE,
                        (*budget)->VfsName()) == SQLITE_OK,
        "the checkpoint uses the same bounded VFS");
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
  if (!db) { return Report(); }
  CHECK(sqlite3_wal_checkpoint_v2(
            db.get(), nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr) == SQLITE_OK,
        "existing payloads are checkpointed before the pressure case");
  const auto before = (*budget)->Inspect();
  CHECK(before && before->ChargedBytes + (*budget)->Reserve() + 4 * kMiB < kLimit &&
            before->ChargedBytes + (*budget)->Reserve() + 8 * kMiB > kLimit,
        "metadata fits, while the new payload's WAL and checkpoint copies do not");
  const auto prepared = Prepare(**cache, {.Id = 99, .Bytes = 4 * kMiB});
  CHECK(prepared.has_value(), "the target package is prepared once");
  if (!prepared) { return Report(); }
  ObserveBodies.store(true, std::memory_order_relaxed);
  const auto first = (*cache)->Publish(*prepared);
  ObserveBodies.store(false, std::memory_order_relaxed);
  CHECK(!first && first.error() == AssetCacheError::Busy,
        "reclamation defers the publication while retaining its prepared package");
  CHECK(BodyStatements.load(std::memory_order_relaxed) == 0,
        "admission defers the package before attempting the large body insert");
  CHECK(Publish(**cache, *prepared), "bounded reclamation eventually admits the complete package");
  const auto after = (*budget)->Inspect();
  CHECK(after && after->ChargedBytes <= kLimit, "the physical hard quota is preserved");
  NearQuotaEstimate();
  return Report();
}
