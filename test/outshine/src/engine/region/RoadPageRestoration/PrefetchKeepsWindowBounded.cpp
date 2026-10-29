#include "StoreRoadManifest.h"
#include "ControlledRoadPageProducer.h"
#include <stop_token>
#include "RoadPageRestoration.h"
#include "Sha256.h"
#include "Check.h"
#include <GeometryProductCodec.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <print>
#include <ratio>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <semaphore>
#include <span>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;

constexpr LongitudeLatitude kAnchor{.LongitudeDeg = 8, .LatitudeDeg = 49};

struct Fixture {
  std::shared_ptr<PreparedRoadPages> Pages;
  std::shared_ptr<const RoadPageManifest> Manifest;
  std::string Key;
};

std::optional<Fixture> Prepare(const std::filesystem::path &directory) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) { return std::nullopt; }
  auto opened = PreparedRoadPages::Open(directory.string());
  if (!opened) { return std::nullopt; }
  std::shared_ptr<PreparedRoadPages> pages = std::move(*opened);
  auto manifest = std::make_shared<RoadPageManifest>();
  manifest->PlanKey.assign(64, 'a');
  manifest->Anchor = kAnchor;
  if (!manifest->Materials.addSurface("streets", Material{})) { return std::nullopt; }
  const auto material = EncodeGeometryProduct(manifest->Materials, 4096);
  if (!material) { return std::nullopt; }
  manifest->MaterialKey = Sha256Hex(material->data(), material->size());
  for (uint32_t index = 0; index < 32; ++index) {
    const RoadPageAddress address{.PlanKey = manifest->PlanKey,
                                  .MaterialKey = manifest->MaterialKey,
                                  .MaterialIndex = 0,
                                  .Anchor = manifest->Anchor,
                                  .Level = 0,
                                  .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = index}}};
    const auto result = pages->Prepare(
        {.Address = address, .Parent = {}, .RequestKey = {}, .NativeBytesMost = 4096},
        outshine::Test::RoadProducer([index](const RoadPageRequest &, const std::stop_token &) {
          const auto x = static_cast<float>(index);
          RoadPageGeometry page;
          page.Mesh.PositionM = {x, 0, 0, x + 1, 0, 0, x, 1, 0};
          page.Mesh.NormalM = {0, 0, 1, 0, 0, 1, 0, 0, 1};
          page.Mesh.ColourRgba = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
          page.Mesh.Index = {0, 1, 2};
          page.Parts = {{.Kind = RoadPagePartKind::Interval, .Index = index}};
          return std::expected<RoadPageGeometry, std::string>(std::move(page));
        }).get());
    if (!result) { return std::nullopt; }
    manifest->Pages.push_back({.Key = result->Key,
                               .Bounds = result->Bounds,
                               .ByteCount = result->ByteCount,
                               .Level = 0,
                               .Parts = address.Parts});
  }
  auto key = ::outshine::Test::StoreRoadManifest(*pages, *manifest);
  if (!key) { return std::nullopt; }
  return Fixture{
      .Pages = std::move(pages), .Manifest = std::move(manifest), .Key = std::move(*key)};
}

bool Barrier(Tasks &pool) {
  const auto barrier = pool.Post([] {});
  return barrier != Tasks::kNoTask && pool.AwaitCompletion(barrier, 5) &&
         pool.TakeCompletion(barrier);
}

bool Ordered(const Geometry &geometry, size_t count) {
  if (!geometry.wellFormed() || geometry.parts() != static_cast<int>(count)) { return false; }
  for (size_t at = 0; at < count; ++at) {
    const auto position = geometry.positionsOf(static_cast<int>(at));
    const auto triangles = geometry.trianglesOf(static_cast<int>(at));
    if (position.size() != 9 || position[0] != static_cast<float>(at) || triangles.size() != 3 ||
        triangles[0] != 0 || triangles[1] != 1 || triangles[2] != 2) {
      return false;
    }
  }
  return true;
}

void CheckPacing(Tasks &pool, const Fixture &fixture) {
  using namespace outshine::Test;
  auto restore = RoadPageRestoration::Begin(pool, fixture.Pages, fixture.Manifest);
  CHECK(restore.has_value(), "the paced reader opens the native page fixture");
  if (!restore) { return; }
  Geometry geometry;
  size_t calls = 0;
  double longestUs = 0;
  const auto began = std::chrono::steady_clock::now();
  bool finished = false;
  while (calls < 1024) {
    const auto before = std::chrono::steady_clock::now();
    const auto advanced = (*restore)->Advance(geometry);
    longestUs = std::max(
        longestUs,
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - before)
            .count());
    ++calls;
    if (!advanced || (*restore)->Missing()) { break; }
    if (*advanced) {
      finished = true;
      break;
    }
    if (!Barrier(pool)) { break; }
  }
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  CHECK(finished && (*restore)->Pages() == fixture.Manifest->Pages.size(),
        "paced worker-ready polling restores every selected page");
  CHECK(Ordered(geometry, fixture.Manifest->Pages.size()),
        "prefetched results retain manifest order and original local triangle indices");
  std::println("ROAD_PACING pages={} calls={} native_bytes={} wall_ms={:.3f} max_advance_us={:.3f}",
               (*restore)->Pages(),
               calls,
               (*restore)->NativeBytes(),
               elapsedMs,
               longestUs);
}

bool ChangeCrc(const std::filesystem::path &directory, const std::string &key, int delta) {
  sqlite3 *raw = nullptr;
  if (sqlite3_open_v2(
          (directory / "assets.sqlite").c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) !=
      SQLITE_OK) {
    if (raw != nullptr) { (void)sqlite3_close(raw); }
    return false;
  }
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, &sqlite3_close);
  sqlite3_stmt *statement = nullptr;
  if (sqlite3_prepare_v2(raw,
                         "UPDATE packages SET crc=crc+?2 WHERE key=(SELECT package FROM assets "
                         "WHERE key=?1)",
                         -1,
                         &statement,
                         nullptr) != SQLITE_OK) {
    return false;
  }
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> update(statement,
                                                                          &sqlite3_finalize);
  return sqlite3_bind_text(
             statement, 1, key.c_str(), static_cast<int>(key.size()), SQLITE_TRANSIENT) ==
             SQLITE_OK &&
         sqlite3_bind_int(statement, 2, delta) == SQLITE_OK &&
         sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(raw) == 1;
}

void CheckWindow(Tasks &pool,
                 const Fixture &fixture,
                 const std::filesystem::path &directory,
                 uint32_t firstExcluded,
                 RoadPageRestoration::Limits limits) {
  using namespace outshine::Test;
  const auto &bad = fixture.Manifest->Pages[firstExcluded].Key;
  CHECK(ChangeCrc(directory, bad, 1),
        "the first page beyond the admission window has a bad payload");

  struct Control {
    std::binary_semaphore Entered{0};
    std::binary_semaphore Release{0};
  };

  const auto control = std::make_shared<Control>();
  const auto blocked = pool.Post([control] {
    control->Entered.release();
    control->Release.acquire();
  });
  const bool running =
      blocked != Tasks::kNoTask && control->Entered.try_acquire_for(std::chrono::seconds(5));
  CHECK(running, "the single native worker is held before any page IO");
  auto restore = RoadPageRestoration::Begin(
      pool, fixture.Pages, fixture.Manifest, std::nullopt, fixture.Key, limits);
  Geometry geometry;
  const auto advanced = restore ? (*restore)->Advance(geometry)
                                : std::expected<bool, std::string>(std::unexpected("no reader"));
  CHECK(advanced && !*advanced && geometry.parts() == 0,
        "polling returns while the IO worker is blocked without publishing a partial page");
  control->Release.release();
  CHECK(Barrier(pool), "all admitted native reads finish before inspecting the manifest");
  const auto retained = fixture.Pages->LoadManifest(fixture.Key);
  CHECK(retained && *retained,
        "neither page count nor byte admission speculatively reads the first excluded payload");
  restore = std::unexpected("released before restoring the excluded payload");
  CHECK(ChangeCrc(directory, bad, -1),
        "the fixed native page is restored after the admission probe");
}

void CheckWindows(Tasks &pool, const Fixture &fixture, const std::filesystem::path &directory) {
  using namespace outshine::Test;
  CheckWindow(
      pool, fixture, directory, 4, {.PendingMost = 4, .NativeBytesMost = size_t{8} * 1024 * 1024});
  CheckWindow(
      pool, fixture, directory, 2, {.PendingMost = 2, .NativeBytesMost = size_t{8} * 1024 * 1024});
  const auto bytes = fixture.Manifest->Pages[0].ByteCount + fixture.Manifest->Pages[1].ByteCount;
  CheckWindow(pool,
              fixture,
              directory,
              2,
              {.PendingMost = 4, .NativeBytesMost = static_cast<size_t>(bytes)});
  CheckWindow(pool, fixture, directory, 1, {.PendingMost = 4, .NativeBytesMost = 1});
  auto serial = RoadPageRestoration::Begin(pool,
                                           fixture.Pages,
                                           fixture.Manifest,
                                           std::nullopt,
                                           {},
                                           {.PendingMost = 4, .NativeBytesMost = 1});
  Geometry geometry;
  bool complete = false;
  for (size_t step = 0; serial && step < 1024; ++step) {
    const auto ready = (*serial)->Advance(geometry);
    if (!ready || (*serial)->Missing()) { break; }
    if (*ready) {
      complete = true;
      break;
    }
    if (!Barrier(pool)) { break; }
  }
  CHECK(complete && Ordered(geometry, fixture.Manifest->Pages.size()),
        "a page larger than the lookahead target progresses alone without a hole or deadlock");
  CHECK(!RoadPageRestoration::Begin(pool,
                                    fixture.Pages,
                                    fixture.Manifest,
                                    std::nullopt,
                                    {},
                                    {.PendingMost = 0, .NativeBytesMost = 1}) &&
            !RoadPageRestoration::Begin(pool,
                                        fixture.Pages,
                                        fixture.Manifest,
                                        std::nullopt,
                                        {},
                                        {.PendingMost = 1, .NativeBytesMost = 0}),
        "empty admission budgets fail before starting any worker");
  const auto excessive = std::make_shared<RoadPageManifest>();
  excessive->PlanKey = fixture.Manifest->PlanKey;
  excessive->MaterialKey = fixture.Manifest->MaterialKey;
  excessive->Anchor = fixture.Manifest->Anchor;
  excessive->Pages = fixture.Manifest->Pages;
  CHECK(excessive->Materials.addSurface("streets",
                                        fixture.Manifest->Materials.surfaceAt(MaterialInstance(0))),
        "the malformed-size probe retains its valid independent material table");
  excessive->Pages[0].ByteCount = size_t{64} * 1024u * 1024u + 1;
  CHECK(!RoadPageRestoration::Begin(pool, fixture.Pages, excessive),
        "oversized native products fail before admission rather than waiting forever");
  excessive->Pages[0].ByteCount = 0;
  CHECK(!RoadPageRestoration::Begin(pool, fixture.Pages, excessive),
        "empty native payload metadata cannot enter the byte window");
  auto empty = RoadPageRestoration::Begin(
      pool, fixture.Pages, fixture.Manifest, std::span<const uint32_t>{});
  Geometry none;
  const auto done = empty ? (*empty)->Advance(none)
                          : std::expected<bool, std::string>(std::unexpected("no reader"));
  CHECK(done && *done && none.parts() == 0 && none.surfaces() == 0,
        "an empty explicit demand completes without IO or material publication");
}
}

int main() {
  using namespace outshine::Test;
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-tests.road-prefetch-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto fixture = Prepare(directory);
  CHECK(fixture.has_value(), "32 independent native pages and their shared manifest are persisted");
  if (fixture) {
    Tasks pool(1);
    CheckPacing(pool, *fixture);
    CheckWindows(pool, *fixture, directory);
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the private native admission fixture is removed");
  return Report();
}
