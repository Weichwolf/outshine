#ifndef OUTSHINE_TEST_ENGINE_GROUNDPUBLICATIONFIXTURE_H
#define OUTSHINE_TEST_ENGINE_GROUNDPUBLICATIONFIXTURE_H
#include "GroundRegionPreparation.h"
#include "PreparedGroundRegions.h"
#include "PreparedRoadPages.h"
#include "PreparedRoadPageFixture.h"
#include "ControlledRoadPageProducer.h"
#include "GeometryProductCodec.h"
#include "GroundRegionAsset.h"
#include "SourceSet.h"
#include "RawSourceCache.h"
#include "Sha256.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace outshine::Test::GroundPublication {
using Regions = Generators::Osm::PreparedGroundRegions;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr size_t kBytesMost = 4096;

struct Fixture {
  std::filesystem::path Directory =
      std::filesystem::temp_directory_path() /
      ("outshine-ground-publication-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Data::RawSourceCache Raw{{.Directory = {}, .Using = Data::RawSourceCache::Use::Off}};
  Data::SourceSet Sources{Raw};
  Tasks Pool{1};
  std::shared_ptr<Regions> Cache;
  std::shared_ptr<Generators::PreparedRoadPages> Pages;
  std::shared_ptr<GroundRegionAsset> Region;
  std::shared_ptr<Generators::WaterAsset> Water;
  std::shared_ptr<Generators::RoadPageManifest> Manifest;
  const std::string Key = Sha256Hex("native ground publication/sources/terrain/parameters");
  const std::string Request = Sha256Hex("native ground publication/demand");
  std::unique_ptr<GroundRegionPreparation> Job;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};

  ~Fixture() {
    if (Job) { Job->Cancel(); }
    const auto barrier = Pool.Post([] {});
    if (barrier != Tasks::kNoTask) { Pool.Wait(barrier); }
    Job.reset();
    Writer.reset();
    Pages.reset();
    Cache.reset();
    std::error_code error;
    std::filesystem::remove_all(Directory, error);
  }

  bool Open(bool roads = false) {
    std::filesystem::create_directories(Directory);
    auto opened = Regions::Open(Directory.string(), Sources, "fixture-rules");
    if (!opened) { return false; }
    Cache = std::move(*opened);
    auto pages = Generators::PreparedRoadPages::Open(Directory.string());
    if (!pages) { return false; }
    Pages = std::move(*pages);
    Region = std::make_shared<GroundRegionAsset>();
    Water = std::make_shared<Generators::WaterAsset>();
    const auto surface = Region->Surfaces.addSurface("ground", Material{});
    if (!surface) { return false; }
    Region->GroundSurface = surface->index();
    const auto part = Region->Surfaces.addPart("ground", *surface);
    const std::array<float, 9> positions{0, 100, 0, 10, 101, 0, 0, 102, 10};
    const std::array<uint32_t, 3> indices{0, 1, 2};
    if (!part || !Region->Surfaces.setPositions(*part, positions) ||
        !Region->Surfaces.setTriangles(*part, indices)) {
      return false;
    }
    Region->Terrain.Sheets = {{.Tile = {.Zoom = 2, .X = 1, .Y = 1},
                               .Nodes = {100, 101, 102, 103},
                               .Side = 2,
                               .Postings = 2}};
    Region->Terrain.Tiles = 1;
    if (roads && !PrepareRoad()) { return false; }
    sqlite3 *connection = nullptr;
    const int status = sqlite3_open((Directory / "assets.sqlite").c_str(), &connection);
    Writer.reset(connection);
    return status == SQLITE_OK;
  }

  bool PrepareRoad() {
    using namespace Generators;
    Manifest = std::make_shared<RoadPageManifest>();
    Manifest->PlanKey = Sha256Hex("native ground publication/transport plan");
    if (!Manifest->Materials.addSurface("streets", Material{})) { return false; }
    const auto material = EncodeGeometryProduct(Manifest->Materials, kBytesMost);
    if (!material) { return false; }
    Manifest->MaterialKey = Sha256Hex(material->data(), material->size());
    const RoadPageAddress address{.PlanKey = Manifest->PlanKey,
                                  .MaterialKey = Manifest->MaterialKey,
                                  .MaterialIndex = 0,
                                  .Anchor = Manifest->Anchor,
                                  .Level = 0,
                                  .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 0}}};
    const auto ready = PrepareAndLoadRoadPage(
        *Pages,
        {.Address = address, .Parent = {}, .RequestKey = {}, .NativeBytesMost = kBytesMost},
        RoadProducer([](const RoadPageRequest &, const std::stop_token &) {
          RoadPageGeometry page;
          page.Mesh.PositionM = {0, 0, 0, 1, 0, 0, 0, 1, 0};
          page.Mesh.NormalM = {0, 0, 1, 0, 0, 1, 0, 0, 1};
          page.Mesh.ColourRgba = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
          page.Mesh.Index = {0, 1, 2};
          page.Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 0}};
          return std::expected<RoadPageGeometry, std::string>(std::move(page));
        }).get());
    if (!ready) { return false; }
    Manifest->Pages.push_back({.Key = ready->Record.Key,
                               .Bounds = ready->Record.Bounds,
                               .ByteCount = ready->NativeBytes,
                               .Level = 0,
                               .Parts = address.Parts});
    Region->SurfaceManifest = RoadPageManifestKey(*Manifest, PreparedRoadPages::ManifestBytesMost);
    return !Region->SurfaceManifest.empty();
  }

  void Hold() const {
    CHECK(sqlite3_exec(Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "an independent SQLite writer holds native publication admission");
  }

  void Release() const {
    CHECK(sqlite3_exec(Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases native publication admission");
  }

  std::optional<std::expected<GroundRegionPreparation::Completed, std::string>> Await() const {
    const auto deadline = std::chrono::steady_clock::now() + kDeadline;
    while (std::chrono::steady_clock::now() < deadline) {
      auto value = Job->Collect();
      if (value) { return value; }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
  }
};

inline bool Busy(const Regions::Failure &error) {
  const auto *cache = std::get_if<AssetCacheError>(&error);
  return cache != nullptr && *cache == AssetCacheError::Busy;
}
}
#endif
