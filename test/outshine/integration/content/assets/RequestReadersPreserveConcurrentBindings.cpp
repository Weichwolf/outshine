#include "test/harness/engine/GroundPublicationFixture.h"
#include "StoreGroundRegion.h"
#include "AssetCache.h"
#include "PreparedRoadPages.h"
#include "PreparedGroundRegions.h"
#include "GroundRegionAsset.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "Sha256.h"
#include "ControlledRoadPageProducer.h"
#include "Shell.h"
#include "Check.h"
#include <cstddef>
#include <dlfcn.h>
#include <filesystem>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
using GroundFixture = GroundPublication::Fixture;
constexpr size_t kBytesMost = 4096;

struct RequestBinding {
  std::string ProductKey, RequestKey;
};

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

int Trace(unsigned event, void * /*context*/, void *statement, void * /*detail*/) {
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

int ReplaceGround(const std::filesystem::path &directory, const RequestBinding &binding) {
  Data::RawSourceCache raw{{.Directory = {}, .Using = Data::RawSourceCache::Use::Off}};
  const Data::SourceSet sources(raw);
  auto cache = Osm::PreparedGroundRegions::Open(directory.string(), sources, "fixture-rules");
  if (!cache) { return 1; }
  auto original = (*cache)->Load(binding.ProductKey);
  if (!original || !*original) { return 1; }
  const auto &loaded = **original;
  const auto stored = StoreGroundRegion(**cache,
                                        std::string(64, 'b'),
                                        GroundRegionBoundsEcef(loaded.Region),
                                        loaded.Region,
                                        loaded.Water,
                                        binding.RequestKey);
  return stored ? 0 : 1;
}

int ReplaceRoad(const std::filesystem::path &directory, const RequestBinding &binding) {
  auto cache = PreparedRoadPages::Open(directory.string());
  if (!cache) { return 1; }
  auto loaded = (*cache)->LoadKey(binding.ProductKey, kBytesMost);
  if (!loaded || !*loaded || !(**loaded).Page) { return 1; }
  auto page = std::move((**loaded).Page);
  auto address = page->Address;
  address.PlanKey = Sha256Hex("concurrently replaced native road plan");
  auto producer =
      RoadProducer([geometry = std::move(page->Geometry)](
                       const RoadPageRequest &, const std::stop_token &) { return geometry; });
  const auto stored = (*cache)->Prepare({.Address = std::move(address),
                                         .Parent = {},
                                         .RequestKey = binding.RequestKey,
                                         .NativeBytesMost = kBytesMost},
                                        producer.get());
  return stored ? 0 : 1;
}

std::string Command(std::string_view executable,
                    std::string_view kind,
                    const std::filesystem::path &directory,
                    const std::string &key,
                    const std::string &request) {
  return ShellArgument(executable) + " " + ShellArgument(kind) + " " +
         ShellArgument(directory.string()) + " " + ShellArgument(key) + " " +
         ShellArgument(request);
}

void CheckGround(std::string_view executable) {
  GroundFixture fixture;
  CHECK(fixture.Open(), "the native ground request fixture opens");
  if (!fixture.Cache || !fixture.Region) { return; }
  const auto stored = StoreGroundRegion(*fixture.Cache,
                                        fixture.Key,
                                        GroundRegionBoundsEcef(*fixture.Region),
                                        *fixture.Region,
                                        *fixture.Water,
                                        fixture.Request);
  CHECK(stored, "a complete native ground product has its original request binding");
  if (!stored) { return; }
  Hook hook((fixture.Directory / "assets.sqlite").string(),
            fixture.Key,
            Command(executable, "--ground", fixture.Directory, fixture.Key, fixture.Request));
  auto reader = Osm::PreparedGroundRegions::Open(
      fixture.Directory.string(), fixture.Sources, "fixture-rules");
  auto records = AssetCache::Open((fixture.Directory / "assets.sqlite").string());
  CHECK(reader && records, "independent native ground and metadata readers open");
  if (!reader || !records) { return; }
  hook.Armed = true;
  auto loaded = (*reader)->LoadRequest(fixture.Request);
  CHECK(hook.Pauses == 1 && hook.ChildStatus == 0,
        "a separate native process replaces the alias while its old ground target is read");
  CHECK(loaded && *loaded && (**loaded).Key == std::string(64, 'b'),
        "the ground request reader returns the current replacement rather than its old target");
  const auto current = (*records)->FindRequest(fixture.Request);
  CHECK(current && *current && (**current).Key == std::string(64, 'b'),
        "the ground request read preserves the replacement binding");
}

void CheckRoad(std::string_view executable) {
  GroundFixture fixture;
  CHECK(fixture.Open(true), "the fixture persists a complete native road page");
  if (!fixture.Pages || !fixture.Manifest || fixture.Manifest->Pages.empty()) { return; }
  const auto key = fixture.Manifest->Pages.front().Key;
  const auto request = Sha256Hex("native road request reader demand");
  auto records = AssetCache::Open((fixture.Directory / "assets.sqlite").string());
  CHECK(records, "the native metadata owner opens");
  if (!records) { return; }
  const auto bound = (*records)->BindRequest(key, request);
  CHECK(bound && *bound, "the native road page receives its initial alias");
  const auto control = fixture.Pages->LoadKey(key, kBytesMost);
  CHECK(control && *control && (**control).Page, "the complete control has its native address");
  if (!control || !*control || !(**control).Page) { return; }
  auto replacement = (**control).Page->Address;
  replacement.PlanKey = Sha256Hex("concurrently replaced native road plan");
  const auto expected = RoadPageKey(replacement);
  Hook hook((fixture.Directory / "assets.sqlite").string(),
            key,
            Command(executable, "--road", fixture.Directory, key, request));
  auto reader = PreparedRoadPages::Open(fixture.Directory.string());
  CHECK(reader, "the independent native road request reader opens");
  if (!reader) { return; }
  hook.Armed = true;
  auto loaded = (*reader)->LoadRequest(request, kBytesMost);
  CHECK(hook.Pauses == 1 && hook.ChildStatus == 0,
        "a separate native process replaces the alias while its old road target is read");
  CHECK(loaded && *loaded && (**loaded).Record.Key == expected &&
            (**loaded).Record.RequestKey == request,
        "the road request reader returns the current replacement and its alias");
  const auto current = (*records)->FindRequest(request);
  CHECK(current && *current && (**current).Key == expected,
        "the road request read cannot restore its obsolete binding");
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
  if (argc == 5) {
    const RequestBinding binding{.ProductKey = argv[3], .RequestKey = argv[4]};
    if (std::string_view(argv[1]) == "--ground") { return ReplaceGround(argv[2], binding); }
    if (std::string_view(argv[1]) == "--road") { return ReplaceRoad(argv[2], binding); }
    return 1;
  }
  CheckGround(argv[0]);
  CheckRoad(argv[0]);
  return Report();
}
