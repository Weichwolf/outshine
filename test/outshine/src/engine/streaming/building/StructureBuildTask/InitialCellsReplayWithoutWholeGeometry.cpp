#include "StructureBuildTask.h"
#include "StructureCellAssembly.h"
#include "PreparedBuildingAssets.h"
#include "PreparedBuildingBasis.h"
#include "SourceSet.h"
#include "BuildingMesh.h"
#include "AssetCache.h"
#include "StructureArtifact.h"
#include "Sha256.h"
#include "Geodesy.h"
#include "Check.h"
#include <array>
#include <atomic>
#include <bit>
#include <latch>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <span>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

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

bool CollectChildren(StructureBuildTask &task,
                     Tasks &pool,
                     const CountingMesh &mesher,
                     const std::weak_ptr<const PreparedBuildingBasis> &warmBasis,
                     bool warm,
                     size_t sourceCount,
                     std::array<std::string, kStructureCellsPerTile> &keys) {
  StructureCellAssembly assembly(task.InitialCells());
  for (;;) {
    const auto &tile = task.Result().Tile;
    if (!tile) { return false; }
    const auto &child = *tile;
    CHECK(child.RequestedCell && !child.RequestedDetail && child.Prints.size() == 1 &&
              child.SelectedView && child.Coordinates && !child.Coordinates->Points().empty(),
          "each bounded product owns complete coordinates and adaptive source selection");
    CHECK(task.Result().CacheHit == warm, "fresh replay uses the exact child cache identity");
    CHECK(child.GeometryAssetKey.size() == 64, "cold and warm products retain the complete key");
    if (child.RequestedCell && *child.RequestedCell > 0 && *child.RequestedCell <= keys.size()) {
      auto &key = keys[*child.RequestedCell - 1u];
      if (warm) {
        CHECK(child.GeometryAssetKey == key,
              "a fresh cache hit preserves the original published key");
      } else {
        key = child.GeometryAssetKey;
      }
    }
    if (warm) {
      const auto basis = warmBasis.lock();
      CHECK(basis && child.Coordinates->Points().data() == basis->PointsLatLon.data() &&
                child.Coordinates->Rings().data() == basis->Holes.data(),
            "each warm child leases complete immutable basis arrays without duplicate copies");
    }
    assembly.Collect(child);
    assembly.Collect(child);
    if (assembly.Remaining() == 0) { break; }
    CHECK(assembly.Metadata().Prints.size() == 1 && assembly.Metadata().Built.UsedBytes() == 0,
          "incomplete source metadata neither duplicates retries nor retains uploaded geometry");
    task.SelectNextCell(static_cast<uint32_t>(std::countr_zero(assembly.Remaining())) + 1u);
    Complete(task, pool, mesher);
    CHECK(task.Result().Status && task.Result().Tile, "the next child completes independently");
    if (!task.Result().Status || !task.Result().Tile) { return false; }
  }
  CHECK(assembly.Metadata().Prints.size() == sourceCount &&
            assembly.Metadata().OccupiedCells == task.InitialCells() && assembly.Triangles() > 0,
        "only the complete source set supplies all contact metadata and triangle counts");
  return true;
}

void Replay(const std::filesystem::path &root,
            bool warm,
            std::array<std::string, kStructureCellsPerTile> &keys) {
  Data::RawSourceCache store({.Directory = (root / "sources").string()});
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
  std::weak_ptr<const PreparedBuildingBasis> warmBasis;
  auto task = [&] {
    if (warm) {
      const auto basis = cache->LoadBasis(std::string(kKey));
      CHECK(basis && *basis, "warm input loads only the owned source basis");
      if (basis) { warmBasis = *basis; }
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
  if (!CollectChildren(task, pool, mesher, warmBasis, warm, raw.Structures.size(), keys)) {
    return;
  }
  CheckCosts(*cache, mesher, warm);
  task.SelectNextCell(static_cast<uint32_t>(std::countr_zero(task.InitialCells())) + 1u);
  task.RequestStop();
  Complete(task, pool, mesher);
  CHECK(!task.Result().Status && !task.Result().Tile,
        "cancelling a restarted child returns no product for source publication");
}

struct CellReference {
  uint32_t Cell = 0;
  std::string Key;
  std::string PayloadDigest;
};

constexpr size_t kParallelRounds = 8;

std::vector<CellReference>
ReadCellReferences(const std::filesystem::path &root,
                   const std::array<std::string, kStructureCellsPerTile> &keys) {
  auto storage = AssetCache::Open((root / "assets.sqlite").string());
  CHECK(storage, "the original cached payloads are independently readable");
  if (!storage) { return {}; }
  std::vector<CellReference> cells;
  for (size_t at = 0; at < keys.size(); ++at) {
    if (keys[at].empty()) { continue; }
    auto payload = (*storage)->Load(keys[at], kStructureArtifactBytesMost);
    CHECK(payload && *payload, "each selected cell has a complete native artifact");
    if (!payload || !*payload) { return {}; }
    const auto bytes = (**payload).Bytes();
    cells.push_back({.Cell = static_cast<uint32_t>(at + 1),
                     .Key = keys[at],
                     .PayloadDigest = Sha256Hex(bytes.data(), bytes.size())});
  }
  return cells;
}

void ReadCells(PreparedBuildingAssets &cache,
               const PreparedBuildingAssets::Basis &basis,
               const RawTile &raw,
               std::span<const CellReference> cells,
               std::span<std::optional<BakedTile>> results,
               std::atomic_uint &failed) {
  for (size_t round = 0; round < kParallelRounds; ++round) {
    for (size_t at = 0; at < cells.size(); ++at) {
      auto view = raw;
      view.RequestedCell = cells[at].Cell;
      auto loaded = cache.LoadGeometry(std::string(kKey), basis, view);
      if (!loaded || !*loaded || (**loaded).GeometryAssetKey != cells[at].Key) {
        failed.fetch_add(1, std::memory_order_relaxed);
        continue;
      }
      results[at] = std::move(**loaded);
    }
  }
}

void ParallelReplay(const std::filesystem::path &root,
                    const std::array<std::string, kStructureCellsPerTile> &keys) {
  Data::RawSourceCache store({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(store);
  auto opened = PreparedBuildingAssets::Open(root.string(), sources);
  CHECK(opened, "parallel geometry readers reopen the prepared native cache");
  if (!opened) { return; }
  auto cache = std::move(*opened);
  auto loadedBasis = cache->LoadBasis(std::string(kKey));
  CHECK(loadedBasis && *loadedBasis, "parallel readers hold an immutable source basis");
  if (!loadedBasis || !*loadedBasis) { return; }
  auto basis = std::move(*loadedBasis);
  const std::weak_ptr<const PreparedBuildingBasis> retained = basis;
  const auto *points = basis->PointsLatLon.data();
  const double first = basis->PointsLatLon.front();
  const auto cells = ReadCellReferences(root, keys);
  CHECK(cells.size() == 2, "parallel readers exercise two independent cell products");
  if (cells.size() != 2) { return; }
  constexpr size_t kWorkers = 4;
  std::array<std::optional<BakedTile>, kWorkers * 2> results;
  std::latch start(static_cast<std::ptrdiff_t>(kWorkers + 1));
  std::atomic_uint failed{0};
  std::vector<std::thread> readers;
  readers.reserve(kWorkers);
  const auto raw = Inputs();
  for (size_t worker = 0; worker < kWorkers; ++worker) {
    readers.emplace_back([&, worker] {
      start.arrive_and_wait();
      ReadCells(*cache,
                basis,
                raw,
                cells,
                std::span(results).subspan(worker * cells.size(), cells.size()),
                failed);
    });
  }
  start.arrive_and_wait();
  const auto invalidated = cache->InvalidateBasis(std::string(kKey));
  for (auto &reader : readers) { reader.join(); }
  CHECK(invalidated && failed == 0,
        "basis invalidation preserves concurrent leased geometry reads");
  const auto costs = cache->Costs();
  CHECK(costs.GeometryHits == kWorkers * kParallelRounds * cells.size() &&
            costs.GeometryMisses == 0 && costs.GeometryWrites == 0,
        "all parallel reads hit existing products without generation or publication");
  cache.reset();
  basis.reset();
  CHECK(!retained.expired(), "decoded cell products retain the basis after cache retirement");
  for (size_t at = 0; at < results.size(); ++at) {
    const auto &tile = results[at];
    CHECK(tile && tile->Coordinates && tile->Coordinates->Points().data() == points &&
              tile->Coordinates->Points().front() == first,
          "every reader retains the same complete immutable coordinate owner");
    if (!tile) { continue; }
    const auto encoded = EncodeStructureArtifact(*tile, tile->GeometryAssetKey);
    CHECK(encoded &&
              Sha256Hex(encoded->data(), encoded->size()) == cells[at % cells.size()].PayloadDigest,
          "parallel decode preserves the complete persisted geometry and selection payload");
  }
  for (auto &tile : results) { tile.reset(); }
  CHECK(retained.expired(), "the last geometry lease releases the immutable source basis");
}

void ReuseWholeProduct(const std::filesystem::path &root) {
  Data::RawSourceCache store({.Directory = (root / "sources").string()});
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
  CHECK(whole, "a valid whole product can be prepared");
  if (!whole) { return; }
  const auto stored = (*cache)->StoreGeometry(std::string(kKey), **base, raw, *whole);
  CHECK(stored, "a valid bounded whole product already exists before initial admission");
  if (!stored) { return; }
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
  CHECK(tile && tile->GeometryAssetKey == *stored,
        "whole-product cache hits retain the actual published key");
  auto clusteredRaw = raw;
  clusteredRaw.ClusterTriangles = 1;
  const auto clustered = BakePreparedStructures(**base, clusteredRaw, mesher, *scratch);
  CHECK(clustered && clustered->Digest == whole->Digest &&
            clustered->Walls.Clusters != whole->Walls.Clusters,
        "different real cluster products can share the old geometry fold");
  if (!clustered) { return; }
  const auto clusteredKey =
      (*cache)->StoreGeometry(std::string(kKey), **base, clusteredRaw, *clustered);
  CHECK(clusteredKey && *clusteredKey != *stored,
        "the complete geometry asset key distinguishes the changed cluster operator");
  const auto loaded = (*cache)->LoadGeometry(std::string(kKey), **base, clusteredRaw);
  CHECK(loaded && *loaded && clusteredKey && (**loaded).GeometryAssetKey == *clusteredKey &&
            (**loaded).Walls.Clusters == clustered->Walls.Clusters,
        "the distinct cluster product replays with its own complete key and partition");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-initial-cells-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::array<std::string, outshine::Generators::kStructureCellsPerTile> keys;
  Replay(root, false, keys);
  Replay(root, true, keys);
  ParallelReplay(root, keys);
  ReuseWholeProduct(root / "existing-whole");
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the isolated generated products are removed");
  return Report();
}
