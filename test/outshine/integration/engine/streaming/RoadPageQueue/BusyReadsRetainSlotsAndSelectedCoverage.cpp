#include "AssetCache.h"
#include "Check.h"
#include "PreparedRoadPages.h"
#include "PreparedRoadPageFixture.h"
#include "ControlledRoadPageProducer.h"
#include "RoadPageQueue.h"
#include "StoreRoadManifest.h"
#include "GeometryProductCodec.h"
#include "Sha256.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <dlfcn.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <stop_token>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kLimit = 4096;
struct TraceHook;
TraceHook *ActiveHook = nullptr;

struct TraceHook {
  std::string Path;
  std::atomic_size_t Reads{0};
  std::atomic_size_t Connections{0};
  explicit TraceHook(std::string path);
  ~TraceHook();
  TraceHook(const TraceHook &) = delete;
  TraceHook &operator=(const TraceHook &) = delete;
};

int Trace(unsigned event, void * /*context*/, void *statement, void * /*detail*/) {
  if (ActiveHook == nullptr || event != SQLITE_TRACE_STMT) { return 0; }
  const auto *sql = sqlite3_sql(static_cast<sqlite3_stmt *>(statement));
  if (sql != nullptr && std::string_view(sql).starts_with("SELECT a.key,a.kind,")) {
    ActiveHook->Reads.fetch_add(1, std::memory_order_relaxed);
  }
  return 0;
}

TraceHook::TraceHook(std::string path) : Path(std::move(path)) {
  ActiveHook = this;
}

TraceHook::~TraceHook() {
  ActiveHook = nullptr;
}

struct Fixture {
  std::filesystem::path Directory =
      std::filesystem::temp_directory_path() /
      ("outshine-road-read-busy-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  TraceHook Hook{(Directory / "assets.sqlite").string()};
  Tasks Pool{1};
  std::shared_ptr<PreparedRoadPages> Pages;
  std::unique_ptr<AssetCache> Records;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};
  RoadPageManifest Manifest;
  std::string ManifestKey;
  std::string Alias = std::string(64, 'c');
  RoadPageAddress Address;
  AssetRecord Record;

  bool Open() {
    std::error_code error;
    std::filesystem::create_directories(Directory, error);
    CHECK(!error, "the fixture creates its private native-cache directory");
    if (error) { return false; }
    Manifest.PlanKey.assign(64, 'a');
    Manifest.Anchor = {.LongitudeDeg = 8, .LatitudeDeg = 49};
    if (!Manifest.Materials.addSurface("streets", Material{})) { return false; }
    const auto material = EncodeGeometryProduct(Manifest.Materials, kLimit);
    if (!material) { return false; }
    Manifest.MaterialKey = Sha256Hex(material->data(), material->size());
    Address = {.PlanKey = Manifest.PlanKey,
               .MaterialKey = Manifest.MaterialKey,
               .MaterialIndex = 0,
               .Anchor = Manifest.Anchor,
               .Level = 0,
               .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 0}}};
    auto pages = PreparedRoadPages::Open(Directory.string());
    auto records = AssetCache::Open((Directory / "assets.sqlite").string());
    CHECK(Hook.Connections.load(std::memory_order_relaxed) >= 2,
          "both native readers install their SQL observer through the real open boundary");
    if (!pages || !records) { return false; }
    Pages = std::move(*pages);
    Records = std::move(*records);
    if (!Seed()) { return false; }
    Manifest.Pages.push_back({.Key = Record.Key,
                              .Bounds = Record.Bounds,
                              .ByteCount = Record.ByteCount,
                              .Level = Record.Level,
                              .Parts = Address.Parts});
    auto stored = StoreRoadManifest(*Pages, Manifest);
    if (!stored) {
      Note(PreparedRoadPages::DescribeFailure(stored.error()).c_str());
      return false;
    }
    ManifestKey = std::move(*stored);
    sqlite3 *database = nullptr;
    const auto status = sqlite3_open((Directory / "assets.sqlite").c_str(), &database);
    Writer.reset(database);
    return status == SQLITE_OK;
  }

  bool Seed() {
    const auto producer = RoadProducer([](const RoadPageRequest &request, const std::stop_token &) {
      RoadPageGeometry page;
      page.Mesh.PositionM = {0, 0, 0, 1, 0, 0, 0, 1, 0};
      page.Mesh.NormalM = {0, 0, 1, 0, 0, 1, 0, 0, 1};
      page.Mesh.ColourRgba = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
      page.Mesh.Index = {0, 1, 2};
      page.Parts = request.Address.Parts;
      return std::expected<RoadPageGeometry, std::string>(std::move(page));
    });
    auto page = PrepareAndLoadRoadPage(
        *Pages,
        {.Address = Address, .Parent = {}, .RequestKey = Alias, .NativeBytesMost = kLimit},
        producer.get());
    if (!page) {
      Note(page.error().c_str());
      return false;
    }
    Record = std::move(page->Record);
    return true;
  }

  bool Corrupt() const {
    auto record = Record;
    const std::array<uint8_t, 4> bytes{1, 2, 3, 4};
    record.Package.clear();
    record.OffsetBytes = 0;
    record.ByteCount = bytes.size();
    return Records->Publish({&record, 1}, bytes).has_value();
  }

  bool Hold() const {
    return sqlite3_exec(Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK;
  }

  void Release() const {
    CHECK(sqlite3_exec(Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases admission");
  }

  void Drain() {
    const auto handle = Pool.Post([] {});
    CHECK(handle != Tasks::kNoTask && Pool.AwaitCompletion(handle, 1),
          "another task can use the same single worker during reader Busy");
    if (handle != Tasks::kNoTask) { (void)Pool.TakeCompletion(handle); }
  }

  ~Fixture() {
    Drain();
    Writer.reset();
    Records.reset();
    Pages.reset();
    std::error_code error;
    std::filesystem::remove_all(Directory, error);
    CHECK(!error, "private native products are removed after their tasks and cache owners end");
  }
};

std::vector<RoadPageQueue::Completed> Wait(RoadPageQueue &queue) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    auto result = queue.Collect();
    if (!result.empty()) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return {};
}

const RoadPageQueue::Product *ReadyProduct(const std::vector<RoadPageQueue::Completed> &done) {
  if (done.size() != 1) { return nullptr; }
  const auto &result = done.front().Result;
  if (!result) { return nullptr; }
  const auto &product = *result;
  return product ? std::addressof(*product) : nullptr;
}

void InvalidPayload(bool cancel) {
  Fixture f;
  const bool opened = f.Open();
  CHECK(opened, "native page, binding and complete manifest prepare");
  if (!opened) { return; }
  CHECK(f.Corrupt() && f.Hold(),
        "an independent writer prevents invalid native payload retirement");
  RoadPageQueue queue(f.Pool, f.Pages, {.PendingMost = 1, .PageBytesMost = kLimit});
  CHECK(queue.RequestCached(f.Record.Key), "the native reader owns one bounded queue slot");
  f.Drain();
  CHECK(queue.Collect().empty() && queue.Pending() == 1,
        "typed invalidation Busy stays pending instead of returning a permanent cache failure");
  CHECK(queue.RequestCached(f.Record.Key) && !queue.RequestMetadata(f.Address),
        "duplicate readers coalesce while result kinds remain distinct");
  f.Drain();
  const auto reads = f.Hook.Reads.load(std::memory_order_relaxed);
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  CHECK(f.Hook.Reads.load(std::memory_order_relaxed) == reads,
        "without caller polling the deferred reader performs no further native SQL reads");
  const auto alias = f.Records->FindRequest(f.Alias);
  CHECK(alias && *alias, "failed writer admission preserves the current request binding");
  if (cancel) {
    queue.Cancel(f.Record.Key);
    const auto done = queue.Collect();
    CHECK(done.size() == 1 && !done.front().Result && queue.Pending() == 0,
          "a cancelled deferred reader releases its slot exactly once without writer admission");
    f.Release();
    f.Drain();
    const auto retained = f.Records->Find(f.Record.Key);
    CHECK(retained && *retained && queue.Collect().empty(),
          "cancelled deferred retirement cannot remove the native row after writer release");
  } else {
    f.Release();
    const auto missing = f.Pages->LoadKey(f.Record.Key, kLimit);
    CHECK(missing && !*missing,
          "the native repair owner retires the invalid payload after writer release");
    CHECK(f.Seed(), "a native producer repairs the current page before reader resumption");
    const auto done = Wait(queue);
    const auto *product = ReadyProduct(done);
    CHECK(product != nullptr && std::holds_alternative<RoadPageLease>(*product) &&
              queue.Pending() == 0,
          "the same native reader resumes the repaired payload and returns one lease");
    const auto retained = f.Pages->LoadManifest(f.ManifestKey);
    CHECK(retained && *retained,
          "a repaired selected page preserves its existing manifest coverage");
  }
}

void MissingSelected(bool repair) {
  Fixture f;
  const bool opened = f.Open();
  CHECK(opened, "the selected-miss case prepares a complete native manifest");
  if (!opened) { return; }
  CHECK(f.Records->Remove(f.Record.Key).has_value() && f.Hold(),
        "the selected payload is absent and another writer blocks manifest retirement");
  RoadPageQueue queue(f.Pool, f.Pages, {.PendingMost = 1, .PageBytesMost = kLimit});
  CHECK(queue.RequestCached(f.Record.Key, {.Key = f.ManifestKey}),
        "selected demand keeps its manifest dependency");
  f.Drain();
  CHECK(queue.Collect().empty() && queue.Pending() == 1,
        "manifest invalidation Busy preserves the admitted selected-miss reader");
  const auto retained = f.Pages->LoadManifest(f.ManifestKey);
  CHECK(retained && *retained, "failed manifest retirement leaves current metadata usable");
  f.Drain();
  f.Release();
  if (repair) { CHECK(f.Seed(), "native selected coverage is repaired before the next admission"); }
  auto done = Wait(queue);
  CHECK(done.size() == 1 && done.front().Result && queue.Pending() == 0,
        "writer release produces exactly one normal terminal read result");
  if (done.size() != 1 || !done.front().Result) { return; }
  CHECK(done.front().Result->has_value() == repair,
        "a repaired native page is a hit and a still absent selected page is an ordinary miss");
  const auto manifest = f.Pages->LoadManifest(f.ManifestKey);
  CHECK(manifest && static_cast<bool>(*manifest) == repair,
        "current selected coverage retains the manifest while unavailable coverage retires it");
}

void WarmMetadataAndBudget() {
  Fixture f;
  const bool opened = f.Open();
  CHECK(opened && f.Hold(), "valid native readers start under a separate held writer");
  if (!opened) { return; }
  RoadPageQueue metadata(f.Pool, f.Pages, {.PendingMost = 1, .PageBytesMost = kLimit});
  CHECK(metadata.RequestMetadata(f.Address), "native metadata demand is admitted");
  auto done = Wait(metadata);
  const auto *product = ReadyProduct(done);
  CHECK(product != nullptr && std::holds_alternative<RoadPageQueue::Receipt>(*product),
        "warm native metadata returns its receipt without writer admission");
  RoadPageQueue payload(f.Pool, f.Pages, {.PendingMost = 1, .PageBytesMost = kLimit});
  CHECK(payload.RequestCached(f.Record.Key), "valid native payload demand is admitted");
  done = Wait(payload);
  CHECK(done.size() == 1 && done.front().Result && *done.front().Result,
        "warm native payload remains readable while another writer is held");
  RoadPageQueue small(f.Pool, f.Pages, {.PendingMost = 1, .PageBytesMost = 1});
  CHECK(small.RequestCached(f.Record.Key),
        "the reader receives its deliberately insufficient native limit");
  done = Wait(small);
  CHECK(done.size() == 1 && !done.front().Result && small.Pending() == 0,
        "a permanent native capacity failure terminates instead of retrying like Busy");
  f.Release();
}
}

extern "C" int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs) {
  using Open = int (*)(const char *, sqlite3 **, int, const char *);
  static const auto open = reinterpret_cast<Open>(dlsym(RTLD_NEXT, "sqlite3_open_v2"));
  if (open == nullptr) { return SQLITE_CANTOPEN; }
  const int status = open(filename, ppDb, flags, zVfs);
  if (status == SQLITE_OK && ActiveHook != nullptr && filename != nullptr &&
      ActiveHook->Path == filename) {
    const int traced = sqlite3_trace_v2(*ppDb, SQLITE_TRACE_STMT, Trace, nullptr);
    if (traced != SQLITE_OK) { return traced; }
    ActiveHook->Connections.fetch_add(1, std::memory_order_relaxed);
  }
  return status;
}

int main() {
  InvalidPayload(false);
  InvalidPayload(true);
  MissingSelected(false);
  MissingSelected(true);
  WarmMetadataAndBudget();
  return outshine::Test::Report();
}
