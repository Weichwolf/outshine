#include "AssetCache.h"
#include "PreparedBuildingAssets.h"
#include "PreparedBuildingBasis.h"
#include "PreparedStructureCodec.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "test/harness/shared/BuildingAssetFixture.h"
#include "Check.h"
#include "Shell.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <dlfcn.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kBytesMost = size_t{64} * 1024 * 1024;

struct Fixture {
  Data::RawSourceCache Raw{{.Directory = {}, .Using = Data::RawSourceCache::Use::Off}};
  Data::SourceSet Sources{Raw};
  std::shared_ptr<PreparedBuildingAssets> Cache;
  std::unique_ptr<AssetCache> Store;
  std::string Key, Request;
  AssetRecord Record;

  bool Open(const std::filesystem::path &directory, bool seed) {
    auto cache = PreparedBuildingAssets::Open(directory.string(), Sources);
    auto store = AssetCache::Open((directory / "assets.sqlite").string());
    if (!cache || !store) { return false; }
    Cache = std::move(*cache);
    Store = std::move(*store);
    const Data::TileId at{.Zoom = 0, .X = 0, .Y = 0};
    Key = Cache->Key(at, 17, {}, 1000, "basis-repair-native-inputs");
    Request = Cache->RequestKey(at, {}, 1000, "basis-repair-native-inputs");
    if (seed) {
      const auto heights = BuildingAssetFixture::Heights();
      if (!heights) { return false; }
      const std::atomic_bool stopped{false};
      const auto generated =
          Cache->Generate(Key, BuildingAssetFixture::Inputs(), *heights, stopped, Request);
      if (!generated || !*generated) { return false; }
    }
    const auto record = Store->FindRequest(Request);
    if (!record || !*record) { return false; }
    Record = **record;
    return true;
  }

  void Corrupt() const {
    const auto ready = Store->Load(Record.Key, kBytesMost);
    CHECK(ready && *ready, "the fixture reads the original complete native basis");
    if (!ready || !*ready) { return; }
    const auto bytes = (**ready).Bytes().first((**ready).Bytes().size() - 1);
    auto record = Record;
    record.Package.clear();
    record.ByteCount = bytes.size();
    CHECK(Store->Publish({&record, 1}, bytes),
          "truncation preserves a valid package CRC but invalidates the native basis");
  }
};

int Repair(const std::filesystem::path &directory) {
  Fixture fixture;
  return fixture.Open(directory, true) ? 0 : 1;
}

struct Hook;
Hook *ActiveHook = nullptr;

struct Hook {
  std::string Path, Key, Command, Output;
  bool Armed = false;
  size_t Pauses = 0;
  int ChildStatus = -1;

  Hook(std::string path, std::string key, std::string command);
  ~Hook();
  Hook(const Hook &) = delete;
  Hook &operator=(const Hook &) = delete;
};

int Trace(unsigned event, void * /*unused*/, void *statement, void * /*unused*/) {
  if (ActiveHook == nullptr || !ActiveHook->Armed || event != SQLITE_TRACE_ROW) { return 0; }
  auto *query = static_cast<sqlite3_stmt *>(statement);
  const auto *sql = sqlite3_sql(query);
  const auto *key = reinterpret_cast<const char *>(sqlite3_column_text(query, 0));
  if (sql == nullptr || key == nullptr ||
      !std::string_view(sql).ends_with(" FROM assets a WHERE a.key=?") || ActiveHook->Key != key) {
    return 0;
  }
  auto &hook = *ActiveHook;
  hook.Armed = false;
  ++hook.Pauses;
  hook.ChildStatus = Run(hook.Command, hook.Output);
  return 0;
}

Hook::Hook(std::string path, std::string key, std::string command)
    : Path(std::move(path)), Key(std::move(key)), Command(std::move(command)) {
  ActiveHook = this;
}

Hook::~Hook() {
  ActiveHook = nullptr;
}

void CheckRepair(std::string_view executable, const std::filesystem::path &directory) {
  Fixture fixture;
  CHECK(fixture.Open(directory, true), "a complete native building asset is persisted");
  if (!fixture.Cache || fixture.Record.Key.empty()) { return; }
  fixture.Corrupt();
  Hook hook((directory / "assets.sqlite").string(),
            fixture.Record.Key,
            ShellArgument(executable) + " --repair " + ShellArgument(directory.string()));
  auto reader = PreparedBuildingAssets::Open(directory.string(), fixture.Sources);
  CHECK(reader, "a fresh actual building reader opens before the old snapshot is armed");
  if (!reader) { return; }
  hook.Armed = true;
  const auto loaded = (*reader)->LoadBasis(fixture.Key);
  CHECK(hook.Pauses == 1 && hook.ChildStatus == 0,
        "a separate process repairs while LoadBasis still holds its old invalid row snapshot");
  CHECK(loaded && *loaded && (*loaded)->Sources.size() == 1,
        "the native reader returns the complete repaired building basis");
  const auto costs = (*reader)->Costs();
  CHECK(costs.BasisHits == 1 && costs.BasisWrites == 0 && costs.ReadBytes == 0,
        "the repaired hit never reconstructs a basis from the legacy base or republishes it");
  const auto alias = fixture.Store->FindRequest(fixture.Request);
  CHECK(alias && *alias && (**alias).Key == fixture.Record.Key,
        "stale invalidation cannot erase the independently repaired durable request binding");
  const auto persisted = fixture.Store->Load(fixture.Record.Key, kBytesMost);
  if (!loaded || !*loaded) { return; }
  const auto expected = EncodePreparedBuildingBasis(**loaded);
  CHECK(persisted && *persisted && expected && std::ranges::equal((**persisted).Bytes(), *expected),
        "the returned native product matches every byte of the committed repaired basis");
}

void CheckInvalidAndForeign(const std::filesystem::path &directory) {
  Fixture fixture;
  CHECK(fixture.Open(directory, true), "the invalidation fixture has native inputs");
  if (!fixture.Cache || fixture.Record.Key.empty()) { return; }
  fixture.Corrupt();
  CHECK(fixture.Store->Remove(fixture.Key), "the native base is absent so it cannot mask a miss");
  const auto invalid = fixture.Cache->LoadBasis(fixture.Key);
  CHECK(invalid && !*invalid, "an unchanged invalid native basis is a missing product");
  const auto absent = fixture.Store->Find(fixture.Record.Key);
  const auto unbound = fixture.Store->FindRequest(fixture.Request);
  CHECK(absent && !*absent && unbound && !*unbound,
        "invalid basis metadata and its request binding retire together");
  auto foreign = fixture.Record;
  foreign.Kind = "independent-product";
  foreign.Package.clear();
  const std::array<uint8_t, 1> bytes{7};
  foreign.ByteCount = bytes.size();
  CHECK(fixture.Store->Publish({&foreign, 1}, bytes),
        "an unrelated producer owns the same address");
  const auto conflict = fixture.Cache->LoadBasis(fixture.Key);
  CHECK(conflict && !*conflict,
        "a foreign product is not a building hit and no absent native base can repair it");
  const auto preserved = fixture.Store->LoadRequest(fixture.Request, kBytesMost);
  CHECK(preserved && *preserved && (**preserved).Record().Kind == foreign.Kind &&
            std::ranges::equal((**preserved).Bytes(), bytes),
        "a building reader preserves a foreign product and its independent request binding");
}

void CheckWriter(const std::filesystem::path &directory) {
  Fixture fixture;
  CHECK(fixture.Open(directory, true), "the writer case publishes an actual native basis");
  if (!fixture.Cache || fixture.Record.Key.empty()) { return; }
  sqlite3 *connection = nullptr;
  const int opened = sqlite3_open((directory / "assets.sqlite").c_str(), &connection);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> writer(connection, sqlite3_close);
  CHECK(opened == SQLITE_OK && writer, "an independent connection opens the writer fixture");
  if (opened != SQLITE_OK || !writer) { return; }
  CHECK(sqlite3_exec(writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the independent connection owns writer admission");
  const auto loaded = fixture.Cache->LoadBasisRequest(fixture.Request);
  CHECK(loaded && *loaded && (**loaded).Product->Sources.size() == 1,
        "a valid native request hit remains readable while another connection holds the writer");
  const auto costs = fixture.Cache->Costs();
  CHECK(costs.BasisHits == 1 && costs.BasisWrites == 1,
        "the valid reader adds no publication beyond the original fixture seed");
  CHECK(sqlite3_exec(writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the independent writer releases admission");
}
}

extern "C" int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs) {
  using Open = int (*)(const char *, sqlite3 **, int, const char *);
  static const auto open = reinterpret_cast<Open>(dlsym(RTLD_NEXT, "sqlite3_open_v2"));
  if (open == nullptr) { return SQLITE_CANTOPEN; }
  const int status = open(filename, ppDb, flags, zVfs);
  if (status == SQLITE_OK && ActiveHook != nullptr && filename != nullptr &&
      ActiveHook->Path == filename) {
    (void)sqlite3_trace_v2(*ppDb, SQLITE_TRACE_ROW, Trace, nullptr);
  }
  return status;
}

int main(int argc, const char *const *argv) {
  if (argc == 3 && std::string_view(argv[1]) == "--repair") { return Repair(argv[2]); }
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-native-building-basis-read-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  CheckRepair(argv[0], directory / "repair");
  CheckInvalidAndForeign(directory / "invalid");
  CheckWriter(directory / "writer");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "all native owners and repair processes finish before private fixture cleanup");
  return Report();
}
