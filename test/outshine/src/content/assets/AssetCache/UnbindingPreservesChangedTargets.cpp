#include "AssetCache.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

struct Fixture {
  std::filesystem::path Directory =
      std::filesystem::temp_directory_path() /
      ("outshine-unbind-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::unique_ptr<AssetCache> Cache;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};

  ~Fixture() {
    Writer.reset();
    Cache.reset();
    std::error_code error;
    std::filesystem::remove_all(Directory, error);
    CHECK(!error, "private products are removed after native owners close");
  }

  bool Open() {
    std::filesystem::create_directories(Directory);
    const auto path = (Directory / "assets.sqlite").string();
    auto opened = AssetCache::Open(path);
    CHECK(opened, "the conditional alias store opens");
    if (!opened) { return false; }
    Cache = std::move(*opened);
    sqlite3 *raw = nullptr;
    const int status = sqlite3_open(path.c_str(), &raw);
    Writer.reset(raw);
    CHECK(status == SQLITE_OK, "an independent writer opens the native store");
    return status == SQLITE_OK;
  }

  void Hold() const {
    CHECK(sqlite3_exec(Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "an independent connection holds writer admission");
  }

  void Release() const {
    CHECK(sqlite3_exec(Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases admission");
  }
};

AssetRecord Record(char key) {
  return {.Key = std::string(64, key),
          .Kind = "native",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = {},
          .ByteCount = 1,
          .Parent = {}};
}

void ConditionalRemoval() {
  Fixture test;
  if (!test.Open()) { return; }
  auto &cache = *test.Cache;
  std::array records{Record('a'), Record('b')};
  const std::array<uint8_t, 1> bytes{7};
  const auto request = std::string(64, 'c');
  const auto neighbor = std::string(64, 'd');
  CHECK(cache.Publish(records, bytes) && cache.BindRequest(records[0].Key, request) &&
            cache.BindRequest(records[1].Key, neighbor),
        "two independent targets share a package and retain separate aliases");
  const auto observed = cache.FindRequest(request);
  CHECK(observed && *observed, "the exact old alias metadata is observed");
  if (!observed || !*observed) { return; }
  test.Hold();
  const auto blocked = cache.UnbindRequestIfMatches(**observed);
  CHECK(!blocked && blocked.error() == AssetCacheError::Busy,
        "matching alias retirement reports writer contention without touching data");
  test.Release();
  CHECK(cache.BindRequest(records[1].Key, request), "a newer owner repairs the request");
  test.Hold();
  const auto changed = cache.UnbindRequestIfMatches(**observed);
  CHECK(changed && !*changed, "a changed target is preserved without writer admission");
  test.Release();
  const auto current = cache.FindRequest(request);
  CHECK(current && *current && (**current).Key == records[1].Key,
        "the newer valid binding remains authoritative");
  if (!current || !*current) { return; }
  records[1].Bounds.Max[0] = 2;
  CHECK(cache.Publish(records, bytes), "the same target receives repaired metadata");
  const auto repaired = cache.UnbindRequestIfMatches(**current);
  CHECK(repaired && !*repaired, "stale metadata cannot retire a repaired target with the same key");
  const auto exact = cache.FindRequest(request);
  if (!exact || !*exact) { return; }
  const auto removed = cache.UnbindRequestIfMatches(**exact);
  const auto absent = cache.FindRequest(request);
  const auto product = cache.Load(records[1].Key, 4096);
  const auto other = cache.LoadRequest(neighbor, 4096);
  CHECK(
      removed && *removed && absent && !*absent && product && *product && other && *other &&
          (**product).Record().Bounds == records[1].Bounds && (**other).Bytes().front() == 7,
      "matching retirement removes only its alias, preserving products, bounds and other aliases");
  test.Hold();
  const auto missing = cache.UnbindRequestIfMatches(**exact);
  CHECK(missing && !*missing, "already absent aliases stay read-only under another writer");
  test.Release();
  const auto invalid = cache.UnbindRequestIfMatches(records[0]);
  CHECK(!invalid && invalid.error() == AssetCacheError::InvalidInput,
        "a product record without a request identity cannot retire an alias");
}
}

int main() {
  ConditionalRemoval();
  return Report();
}
