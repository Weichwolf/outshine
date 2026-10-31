#include "AssetCache.h"
#include "Check.h"
#include "Shell.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <zlib.h>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kBytesMost = 4096;

AssetRecord Record(bool neighbor = false) {
  return {.Key = std::string(64, neighbor ? 'b' : 'a'),
          .Kind = neighbor ? "independent-product" : "native-test",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = {},
          .ByteCount = 1,
          .Parent = {},
          .RequestKey = std::string(64, neighbor ? 'd' : 'c')};
}

std::expected<void, AssetCacheError>
Publish(AssetCache &cache, uint8_t byte, bool neighbor = false) {
  const std::array records{Record(), Record(true)};
  const std::array bytes{byte};
  return cache.Publish(std::span(records).first(neighbor ? 2 : 1), bytes);
}

struct Fixture {
  std::filesystem::path Directory =
      std::filesystem::temp_directory_path() /
      ("outshine-validated-cache-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::string Path = (Directory / "assets.sqlite").string();
  std::unique_ptr<AssetCache> Cache;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};

  ~Fixture() {
    Writer.reset();
    Cache.reset();
    std::error_code error;
    std::filesystem::remove_all(Directory, error);
    CHECK(!error, "the fixture closes native cache owners before removing private products");
  }

  bool Open() {
    std::filesystem::create_directories(Directory);
    auto opened = AssetCache::Open(Path);
    if (!opened) { return false; }
    Cache = std::move(*opened);
    sqlite3 *connection = nullptr;
    const int status = sqlite3_open(Path.c_str(), &connection);
    Writer.reset(connection);
    return status == SQLITE_OK;
  }

  void Hold() const {
    CHECK(sqlite3_exec(Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "an independent SQLite connection owns writer admission");
  }

  void Release() const {
    CHECK(sqlite3_exec(Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases admission");
  }

  void Corrupt(bool crcValid) const {
    const std::array<uint8_t, 1> bad{0};
    const auto checksum = crc32_z(0, bad.data(), bad.size());
    const auto sql =
        "UPDATE package_bodies SET bytes=X'00';" +
        (crcValid ? "UPDATE packages SET crc=" + std::to_string(checksum) : std::string{});
    CHECK(sqlite3_exec(Writer.get(), sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
          "physical corruption preserves the original package identity and asset metadata");
  }
};

bool Good(const CachedAsset &asset) {
  return asset.Bytes().size() == 1 && asset.Bytes().front() == 7;
}

int Child(const std::string &path, bool busy) {
  auto cache = AssetCache::Open(path);
  if (!cache) { return 1; }
  const auto result = Publish(**cache, 7);
  if (busy) { return !result && result.error() == AssetCacheError::Busy ? 0 : 2; }
  if (!result) { return 3; }
  const auto ready = (*cache)->Load(Record().Key, kBytesMost);
  return ready && *ready && Good(**ready) ? 0 : 4;
}

void RunChild(std::string_view executable, const Fixture &fixture, bool busy) {
  std::string said;
  const auto command =
      ShellArgument(executable) + (busy ? " --busy " : " --repair ") + ShellArgument(fixture.Path);
  const int status = Run(command, said);
  CHECK(status == 0,
        busy ? "a fresh native process cannot publish during revalidation"
             : "a fresh native process repairs and validates before writer admission");
}

void CheckRepair(std::string_view executable, bool sameIdentity) {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "the repair case opens a private native cache");
  if (!opened) { return; }
  CHECK(Publish(*fixture.Cache, sameIdentity ? 7 : 0), "the initial opaque native record exists");
  if (sameIdentity) { fixture.Corrupt(true); }
  const auto before = fixture.Cache->Find(Record().Key);
  size_t calls = 0;
  const auto loaded = fixture.Cache->LoadValidated(
      Record().Key, "native-test", kBytesMost, [&](const CachedAsset &asset) {
        ++calls;
        if (calls == 1) { RunChild(executable, fixture, false); }
        return Good(asset);
      });
  CHECK(loaded && *loaded && Good(**loaded) && calls == 2,
        "an invalid old read is reloaded and validated after an independent process repairs it");
  const auto after = fixture.Cache->Find(Record().Key);
  CHECK(before && *before && after && *after &&
            (sameIdentity ? **before == **after : (**before).Package != (**after).Package),
        "repair is preserved both with changed content identity and identical normalized metadata");
  const auto alias = fixture.Cache->LoadRequest(std::string(64, 'c'), kBytesMost);
  CHECK(alias && *alias && Good(**alias), "the repaired product keeps its durable request binding");
}

void CheckWriterAndNeighbors(std::string_view executable) {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "the revalidation case opens a private native cache");
  if (!opened) { return; }
  CHECK(Publish(*fixture.Cache, 0, true), "two independent records share the same stored package");
  size_t calls = 0;
  const auto absent = fixture.Cache->LoadValidated(
      Record().Key, "native-test", kBytesMost, [&](const CachedAsset &asset) {
        ++calls;
        if (calls == 2) { RunChild(executable, fixture, true); }
        return Good(asset);
      });
  CHECK(absent && !*absent && calls == 2,
        "a still invalid product is removed only after validation with exclusive writer admission");
  const auto alias = fixture.Cache->FindRequest(std::string(64, 'c'));
  const auto neighbor = fixture.Cache->LoadRequest(std::string(64, 'd'), kBytesMost);
  const auto selected = fixture.Cache->Select({.Bounds = Record().Bounds,
                                               .Kind = {},
                                               .Level = std::nullopt,
                                               .Radius = std::nullopt,
                                               .Planes = {}});
  CHECK(alias && !*alias && neighbor && *neighbor && (**neighbor).Bytes().front() == 0 &&
            selected && selected->size() == 1 && selected->front().Key == Record(true).Key,
        "row, bounds and alias are removed atomically while another member and its alias survive");
}

void CheckReadAdmissionAndErrors() {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "the admission case opens a private native cache");
  if (!opened) { return; }
  CHECK(Publish(*fixture.Cache, 7), "a complete native hit is committed");
  fixture.Hold();
  const auto ready = fixture.Cache->LoadValidated(Record().Key, "native-test", kBytesMost, Good);
  CHECK(ready && *ready && Good(**ready), "normal valid hits do not acquire the SQLite writer");
  const auto failed =
      fixture.Cache->LoadValidated(Record().Key,
                                   "native-test",
                                   kBytesMost,
                                   [](const CachedAsset &) -> std::expected<bool, AssetCacheError> {
                                     return std::unexpected(AssetCacheError::CapacityExceeded);
                                   });
  CHECK(!failed && failed.error() == AssetCacheError::CapacityExceeded,
        "validator errors remain typed and cannot turn valid data into an invalidation");
  const auto foreign = fixture.Cache->LoadValidated(Record().Key, "other-kind", kBytesMost, Good);
  CHECK(!foreign && foreign.error() == AssetCacheError::InvalidInput,
        "a foreign native kind remains a permanent non-destructive failure");
  fixture.Release();
  fixture.Corrupt(true);
  fixture.Hold();
  const auto busy = fixture.Cache->LoadValidated(Record().Key, "native-test", kBytesMost, Good);
  CHECK(!busy && busy.error() == AssetCacheError::Busy,
        "a genuinely invalid read retains typed Busy when another writer prevents revalidation");
  const auto retained = fixture.Cache->FindRequest(std::string(64, 'c'));
  CHECK(retained && *retained, "failed admission preserves row and alias for later repair");
  fixture.Release();
}

void CheckBrokenPackage() {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "the corrupt package case opens a private native cache");
  if (!opened) { return; }
  CHECK(Publish(*fixture.Cache, 7), "the CRC-protected native payload exists");
  fixture.Corrupt(false);
  size_t calls = 0;
  const auto absent = fixture.Cache->LoadValidated(
      Record().Key, "native-test", kBytesMost, [&](const CachedAsset &asset) {
        ++calls;
        return Good(asset);
      });
  CHECK(absent && !*absent && calls == 0,
        "missing or CRC-invalid packages are rechecked and removed without native decoding");
  const auto gone = fixture.Cache->Find(Record().Key);
  CHECK(gone && !*gone, "a corrupt package cannot leave a false metadata hit");
}
}

int main(int argc, char *const *argv) {
  if (argc == 3) { return Child(argv[2], std::string_view(argv[1]) == "--busy"); }
  CheckRepair(argv[0], false);
  CheckRepair(argv[0], true);
  CheckWriterAndNeighbors(argv[0]);
  CheckReadAdmissionAndErrors();
  CheckBrokenPackage();
  return outshine::Test::Report();
}
