#include "StructureBuildTask.h"
#include "StructureCellAssembly.h"
#include "PreparedBuildingAssets.h"
#include "SourceSet.h"
#include "BuildingMesh.h"
#include "Geodesy.h"
#include "Check.h"
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr std::string_view kKey =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

class CountingMesh final : public BuildingMesh {
public:
  mutable size_t Calls = 0;

  std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &scratch, Raised &into) const noexcept override {
    ++Calls;
    return BuildingMesh::Mesh(plan, scratch, into);
  }
};

RawTile Inputs() {
  constexpr double kLon = 9;
  constexpr double kLat = 47;
  constexpr double kStep = 0.008;
  constexpr double kSize = 0.0001;
  constexpr double kTileSpanM = 2000;
  constexpr double kFocalPx = 720;
  RawTile raw;
  raw.TileSpanM = kTileSpanM;
  raw.Eye = {.LongitudeDeg = kLon, .LatitudeDeg = kLat};
  raw.Projection = {.FocalPx = kFocalPx, .AllowedErrorPx = 1};
  GeoToEcef({.LongitudeDeg = kLon, .LatitudeDeg = kLat}, raw.AnchorEcef);
  const Ground::GeoBounds bounds{.MinLonDeg = kLon,
                                 .MinLatDeg = kLat,
                                 .MaxLonDeg = kLon + 2 * kStep,
                                 .MaxLatDeg = kLat + 2 * kStep};
  for (uint32_t at = 0; at < 2; ++at) {
    const double lon = kLon + at * kStep;
    const double lat = kLat + at * kStep;
    const std::array ring{lat, lon, lat, lon + kSize, lat + kSize, lon + kSize, lat + kSize, lon};
    const auto cell = StructureCellOf(bounds, ring);
    CHECK(cell, "each source has a valid distinct cell");
    if (!cell) { return {}; }
    raw.LatLon.insert_range(raw.LatLon.end(), ring);
    raw.Structures.push_back({.LocalFirst = at * 4,
                              .PointCount = 4,
                              .Cell = *cell,
                              .HeightM = 12,
                              .Pitched = 0,
                              .WallColour = std::nullopt,
                              .Facade = std::nullopt});
  }
  return raw;
}

void Complete(StructureBuildTask &task, Tasks &pool, const StructureMesher &mesher) {
  task.Start(pool, mesher);
  for (size_t step = 0; step < 128; ++step) {
    task.Join(pool);
    if (!task.Result().Status || task.Result().Tile) { return; }
    task.Resume(pool, mesher);
  }
  task.RequestStop();
  task.Join(pool);
  CHECK(false, "the bounded producer must finish without exhausting its slice limit");
}

void CheckCosts(const PreparedBuildingAssets &cache, const CountingMesh &mesher, bool warm) {
  const auto costs = cache.Costs();
  CHECK(warm ? costs.GeometryHits == 2 && costs.GeometryWrites == 0 && costs.Hits == 0 &&
                   costs.ReadBytes == 0 && mesher.Calls == 0
             : costs.GeometryWrites == 2 && mesher.Calls > 0,
        "cold writes only children; a fresh warm producer never loads plans or invokes meshing");
}

void Replay(const std::filesystem::path &root, bool warm) {
  Data::ContentStore store({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(store);
  auto opened = PreparedBuildingAssets::Open(root.string(), sources);
  CHECK(opened, "the native asset store opens");
  if (!opened) { return; }
  auto cache = *opened;
  const auto raw = Inputs();
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  CountingMesh mesher;
  Tasks pool(1);
  auto task = [&] {
    if (warm) {
      const auto basis = cache->LoadBasis(std::string(kKey));
      CHECK(basis && *basis, "warm input loads only the owned source basis");
      return StructureBuildTask(0,
                                basis ? *basis : nullptr,
                                std::make_unique<RawTile>(raw),
                                std::make_unique<StructureBuildTask::Output>(),
                                mesher.Scratch());
    }
    return StructureBuildTask(0,
                              std::make_unique<RawTile>(raw),
                              Ground::HeightField::Of(0, {block}),
                              std::make_unique<StructureBuildTask::Output>(),
                              mesher.Scratch());
  }();
  task.UseNativeAssets(cache, std::string(kKey));
  task.UseInitialCells();
  Complete(task, pool, mesher);
  CHECK(task.Result().Status && task.Result().Tile && std::popcount(task.InitialCells()) == 2,
        "initial production chooses children before emitting any whole geometry");
  if (!task.Result().Status || !task.Result().Tile) { return; }
  StructureCellAssembly assembly(task.InitialCells());
  for (;;) {
    const auto &tile = task.Result().Tile;
    if (!tile) { return; }
    const auto &child = *tile;
    CHECK(child.RequestedCell && !child.RequestedDetail && child.Prints.size() == 1 &&
              child.SelectedView && child.Coordinates && !child.Coordinates->Points.empty(),
          "each bounded product owns complete coordinates and adaptive source selection");
    CHECK(task.Result().CacheHit == warm, "fresh replay uses the exact child cache identity");
    assembly.Collect(child);
    assembly.Collect(child);
    if (assembly.Remaining() == 0) { break; }
    CHECK(assembly.Metadata().Prints.size() == 1 && assembly.Metadata().Built.UsedBytes() == 0,
          "incomplete source metadata neither duplicates retries nor retains uploaded geometry");
    task.SelectNextCell(static_cast<uint32_t>(std::countr_zero(assembly.Remaining())) + 1u);
    Complete(task, pool, mesher);
    CHECK(task.Result().Status && task.Result().Tile, "the next child completes independently");
    if (!task.Result().Status || !task.Result().Tile) { return; }
  }
  CHECK(assembly.Metadata().Prints.size() == raw.Structures.size() &&
            assembly.Metadata().OccupiedCells == task.InitialCells() && assembly.Triangles() > 0,
        "only the complete source set supplies all contact metadata and triangle counts");
  CheckCosts(*cache, mesher, warm);
  task.SelectNextCell(static_cast<uint32_t>(std::countr_zero(task.InitialCells())) + 1u);
  task.RequestStop();
  Complete(task, pool, mesher);
  CHECK(!task.Result().Status && !task.Result().Tile,
        "cancelling a restarted child returns no product for source publication");
}

void ReuseWholeProduct(const std::filesystem::path &root) {
  Data::ContentStore store({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(store);
  auto cache = PreparedBuildingAssets::Open(root.string(), sources);
  CHECK(cache, "a separate cache opens for an existing whole product");
  if (!cache) { return; }
  const auto raw = Inputs();
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  const std::atomic_bool stopping(false);
  auto base = (*cache)->Generate(std::string(kKey), raw, *heights, stopping);
  CHECK(base && *base, "the complete enriched source is available");
  if (!base || !*base) { return; }
  CountingMesh mesher;
  auto scratch = mesher.Scratch();
  const auto whole = BakePreparedStructures(**base, raw, mesher, *scratch);
  CHECK(whole && (*cache)->StoreGeometry(std::string(kKey), **base, raw, *whole),
        "a valid bounded whole product already exists before initial admission");
  const auto basis = (*cache)->LoadBasis(std::string(kKey));
  CHECK(basis && *basis, "the warm source basis is available without the full plans");
  if (!basis || !*basis) { return; }
  mesher.Calls = 0;
  Tasks pool(1);
  StructureBuildTask task(0,
                          *basis,
                          std::make_unique<RawTile>(raw),
                          std::make_unique<StructureBuildTask::Output>(),
                          mesher.Scratch());
  task.UseNativeAssets(*cache, std::string(kKey));
  task.UseInitialCells();
  Complete(task, pool, mesher);
  const auto &tile = task.Result().Tile;
  CHECK(task.Result().Status && tile && task.Result().CacheHit && task.InitialCells() == 0 &&
            !tile->RequestedCell && tile->Prints.size() == raw.Structures.size() &&
            mesher.Calls == 0 && (*cache)->Costs().GeometryWrites == 1,
        "initial admission reuses a valid whole cache hit without rebuilding it as children");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-initial-cells-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Replay(root, false);
  Replay(root, true);
  ReuseWholeProduct(root / "existing-whole");
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the isolated generated products are removed");
  return Report();
}
