#include "AssetCache.h"
#include "PreparedTerrainAssets.h"
#include "PreparedGroundPatchCodec.h"
#include "GroundPatch.h"
#include "GroundSample.h"
#include "SourceSet.h"
#include "RawSourceCache.h"
#include "Check.h"
#include "Shell.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

struct sqlite3_api_routines;

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr Data::TileId kAt{.Zoom = 14, .X = 8937, .Y = 5683};

std::shared_ptr<const GroundPatch> Patch() {
  std::array<GroundPatch::Posting, 9> postings;
  for (auto &posting : postings) { posting.Height = GroundSample::At(100.12345); }
  return GroundPatch::Complete(Tile(kAt.Zoom, kAt.X, kAt.Y), 3, postings);
}

bool Save(PreparedTerrainAssets &cache, const GroundPatch &patch) {
  const auto key = cache.PatchKey(kAt, {}, 3, 13);
  auto write = cache.PreparePatch(key, kAt, patch);
  if (!write) { return false; }
  const auto ready = cache.StorePatch(*write);
  return ready && *ready;
}

int Repair(const std::string &directory) {
  Data::RawSourceCache raw{{.Directory = {}, .Using = Data::RawSourceCache::Use::Off}};
  const Data::SourceSet sources(raw);
  auto cache = PreparedTerrainAssets::Open(directory, sources);
  const auto patch = Patch();
  return cache && patch && Save(**cache, *patch) ? 0 : 1;
}

struct Hook;
Hook *ActiveHook = nullptr;

struct Hook {
  std::string Path;
  std::string Command;
  std::string Output;
  bool Armed = false;
  size_t Pauses = 0;
  int ChildStatus = -1;

  Hook(std::string path, std::string command);
  ~Hook();
  Hook(const Hook &) = delete;
  Hook &operator=(const Hook &) = delete;
};

int Trace(unsigned event, void * /*context*/, void *statement, void * /*detail*/) {
  if (ActiveHook == nullptr) { return 0; }
  auto &hook = *ActiveHook;
  const auto *sql = sqlite3_sql(static_cast<sqlite3_stmt *>(statement));
  if (event != SQLITE_TRACE_ROW || !hook.Armed || sql == nullptr ||
      !std::string_view(sql).starts_with("SELECT a.key,a.kind,")) {
    return 0;
  }
  hook.Armed = false;
  ++hook.Pauses;
  hook.ChildStatus = Run(hook.Command, hook.Output);
  return 0;
}

int Install(sqlite3 *database,
            [[maybe_unused]] char **error,
            [[maybe_unused]] const sqlite3_api_routines *api) {
  const auto *path = sqlite3_db_filename(database, "main");
  if (ActiveHook == nullptr || path == nullptr || ActiveHook->Path != path) { return SQLITE_OK; }
  return sqlite3_trace_v2(database, SQLITE_TRACE_ROW, Trace, nullptr);
}

Hook::Hook(std::string path, std::string command)
    : Path(std::move(path)), Command(std::move(command)) {
  ActiveHook = this;
  CHECK(sqlite3_auto_extension(reinterpret_cast<void (*)()>(Install)) == SQLITE_OK,
        "the fixture traces actual native SQLite connections without a product hook");
}

Hook::~Hook() {
  (void)sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(Install));
  ActiveHook = nullptr;
}

void CheckConcurrentRepair(std::string_view executable, const std::filesystem::path &directory) {
  Data::RawSourceCache raw{{.Directory = {}, .Using = Data::RawSourceCache::Use::Off}};
  const Data::SourceSet sources(raw);
  const auto path = (directory / "assets.sqlite").string();
  const auto patch = Patch();
  auto seed = PreparedTerrainAssets::Open(directory.string(), sources);
  auto storage = AssetCache::Open(path);
  CHECK(seed && storage && patch && Save(**seed, *patch),
        "the fixture commits and validates a real native sampling product");
  if (!seed || !storage || !patch) { return; }
  const auto key = (*seed)->PatchKey(kAt, {}, 3, 13);
  auto bytes = EncodePreparedGroundPatch(kAt, *patch);
  auto record = (*storage)->Find(key);
  if (!bytes || !record || !*record) {
    CHECK(false, "the original native record exists");
    return;
  }
  bytes->pop_back();
  (**record).Package.clear();
  (**record).ByteCount = bytes->size();
  CHECK((*storage)->Publish({&**record, 1}, *bytes),
        "the first native read will observe a semantically truncated payload with intact CRC");
  Hook hook(path, ShellArgument(executable) + " --repair " + ShellArgument(directory.string()));
  auto reader = PreparedTerrainAssets::Open(directory.string(), sources);
  CHECK(reader.has_value(), "the actual reader opens before its adversarial read is armed");
  if (!reader) { return; }
  hook.Armed = true;
  const auto ready = (*reader)->LoadPatch(key, kAt, 3);
  CHECK(
      hook.Pauses == 1 && hook.ChildStatus == 0,
      "a fresh native process repairs while the actual LoadPatch still holds its old row snapshot");
  CHECK(ready && *ready && (*reader)->Costs().PatchDecodes == 2,
        "the full native caller rejects its stale payload, revalidates and returns the repaired "
        "patch");
  const auto expected = EncodePreparedGroundPatch(kAt, *patch);
  const auto restored = ready && *ready ? EncodePreparedGroundPatch(kAt, **ready) : std::nullopt;
  CHECK(expected && restored && *expected == *restored,
        "the repaired native product retains every original sample bit and geographic parameter");
  const auto retained = (*storage)->Load(key, kPreparedGroundPatchBytesMost);
  CHECK(retained && *retained && DecodePreparedGroundPatch(kAt, 3, (**retained).Bytes()),
        "the old invalidation cannot delete the concurrently committed valid native hit");
}
}

int main(int argc, char **argv) {
  if (argc == 3) { return Repair(argv[2]); }
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-native-patch-repair-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  CheckConcurrentRepair(argv[0], directory);
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "fresh processes and native reader owners finish before private input cleanup");
  return outshine::Test::Report();
}
