#include "PreparedStructureTile.h"
#include "PreparedBuildingAssets.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include "StructureBuildTask.h"
#include "Tasks.h"
#include "PreparedStructureCodec.h"
#include "PreparedStructurePlan.h"
#include "content/AssetGeneration.h"
#include "Sha256.h"
#include "BuildingScratch.h"
#include "BuildingMesh.h"
#include "Check.h"
#include "Geodesy.h"
#include "math/Units.h"
#include <array>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <chrono>
#include <string>
#include <memory>
#include <vector>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

RawTile Inputs() {
  RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47}, raw.AnchorEcef);
  const Ground::GeoBounds region{
      .MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.02, .MaxLatDeg = 47.02};
  for (uint32_t index = 0; index < 3; ++index) {
    const double lon = 9.001 + static_cast<double>(index) * 0.002;
    const std::array ring{47.001, lon, 47.001, lon + 0.0002, 47.0012, lon + 0.0002, 47.0012, lon};
    const auto cell = StructureCellOf(region, ring);
    CHECK(cell.has_value(), "prepared fixture has a native cell");
    if (!cell) { return {}; }
    const auto first = static_cast<uint32_t>(raw.LatLon.size() / 2);
    raw.LatLon.insert(raw.LatLon.end(), ring.begin(), ring.end());
    raw.Structures.push_back({.LocalFirst = first,
                              .PointCount = 4,
                              .SourceFirst = first,
                              .Cell = *cell,
                              .HeightM = index == 0 ? 0.0 : 12.0,
                              .MinimumHeightM = index == 2 ? 3.0 : 0.0,
                              .Pitched = 0});
  }
  raw.Ways.push_back({.LocalFirst = static_cast<uint32_t>(raw.LatLon.size() / 2),
                      .PointCount = 2,
                      .HalfWidthM = 3.0f});
  raw.LatLon.insert(raw.LatLon.end(), {47.00096, 9.0, 47.00096, 9.02});
  return raw;
}

std::shared_ptr<const Ground::HeightField> Heights() {
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {125.125f, 131.25f, 142.5f, 149.75f};
  return Ground::HeightField::Of(0, {block});
}

void NativeLODWithoutPlans(const std::filesystem::path &root,
                           const std::string &key,
                           const std::shared_ptr<PreparedBuildingAssets> &cache,
                           const RawTile &view,
                           const BakedTile &reference) {
  const auto basis = cache->LoadBasis(key);
  CHECK(basis && *basis && (*basis)->Sources.size() == 3,
        "native basis is complete without plan or surface arrays");
  if (!basis || !*basis) { return; }
  auto database = AssetCache::Open((root / "assets.sqlite").string());
  CHECK(database && (*database)->Remove(key), "fixture removes the complete plan product");
  if (!database) { return; }
  const auto before = cache->Costs();
  BuildingMesh mesher;
  Tasks pool(1);
  StructureBuildTask task(0,
                          *basis,
                          std::make_unique<RawTile>(view),
                          std::make_unique<StructureBuildTask::Output>(),
                          mesher.Scratch());
  task.UseNativeAssets(cache, key);
  task.Start(pool, mesher);
  task.Join(pool);
  CHECK(task.Result().Status && task.Result().Tile && task.Result().CacheHit &&
            task.Result().Tile->Digest == reference.Digest &&
            task.Result().Tile->Prints == reference.Prints &&
            task.Result().Tile->Coordinates->Points == (*basis)->PointsLatLon &&
            task.PreparedBase() == nullptr && cache->Costs().ReadBytes == before.ReadBytes,
        "a cached LOD and its owned contacts load with no full plan product or source inputs");
  RawTile moved = view;
  moved.Eye.LongitudeDeg += 0.01;
  StructureBuildTask miss(0,
                          *basis,
                          std::make_unique<RawTile>(moved),
                          std::make_unique<StructureBuildTask::Output>(),
                          mesher.Scratch());
  miss.UseNativeAssets(cache, key);
  miss.Start(pool, mesher);
  miss.Join(pool);
  const auto *kind = miss.Result().Status
                         ? nullptr
                         : std::get_if<StructureBakeErrorKind>(&miss.Result().Status.error());
  CHECK(kind && *kind == StructureBakeErrorKind::NativeInputsMissing && !miss.Result().Tile,
        "a missing required plan requests source preparation instead of publishing empty geometry");
  const auto again = cache->LoadBasis(key);
  CHECK(again && !*again, "invalidated basis returns to the generator miss path");
  const std::atomic_bool stopping{false};
  const auto generated = cache->Generate(key, Inputs(), *Heights(), stopping);
  CHECK(generated && *generated && cache->LoadBasis(key),
        "the same miss can regenerate complete inputs and restore native delivery");
}

void NativeCache(const std::filesystem::path &root) {
  Data::ContentStore store({.Directory = (root / "sources").string()});
  Data::SourceSet sources(store);
  const auto directory = root.string();
  auto opened = PreparedBuildingAssets::Open(directory, sources);
  CHECK(opened.has_value(), "native building service opens the shared spatial cache");
  if (!opened) { return; }
  const Data::TileId tile{.Zoom = 0, .X = 0, .Y = 0};
  const Ground::ShapedGround shape;
  const auto request = (*opened)->RequestKey(tile, shape, 1000);
  const auto key = (*opened)->Key(tile, 17, shape, 1000);
  const auto miss = (*opened)->Load(key);
  CHECK(miss && !*miss, "native base miss is distinct from storage failure");
  const std::atomic_bool stopping{false};
  const auto generated = (*opened)->Generate(key, Inputs(), *Heights(), stopping, request);
  CHECK(generated && *generated && (*generated)->Structures.size() == 3,
        "cache miss publishes and reloads complete intrinsic building assets");
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  RawTile view;
  view.RequestedDetail = LevelOfDetail::Shell;
  view.Eye = {.LongitudeDeg = 10, .LatitudeDeg = 47};
  const auto geometry = generated && *generated
                            ? BakePreparedStructures(**generated, view, mesher, *scratch)
                            : std::expected<BakedTile, StructureBakeError>(
                                  std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct));
  CHECK(geometry.has_value(), "native basis supplies the reusable selected LOD geometry");
  if (!geometry || !generated || !*generated) { return; }
  CHECK((*opened)->StoreGeometry(key, **generated, view, *geometry).has_value(),
        "native selected geometry is published beside the enriched base");
  opened->reset();
  auto restarted = PreparedBuildingAssets::Open(directory, sources);
  CHECK(restarted.has_value(), "building cache service survives a new connection");
  if (!restarted) { return; }
  const auto demanded = (*restarted)->LoadBasisRequest(request);
  CHECK(demanded && *demanded && (**demanded).BaseKey == key && (**demanded).Product &&
            (**demanded).Product->Sources.size() == 3 && (*restarted)->Costs().ReadBytes == 0 &&
            (*restarted)->Costs().BasisWrites == 0,
        "a fresh native demand resolves content identity and basis without source inputs or plans");
  const auto hit = (*restarted)->Load(key);
  CHECK(hit && *hit && (*hit)->Structures.size() == 3 && (*restarted)->Costs().Writes == 0 &&
            (*restarted)->Costs().Hits == 1,
        "native hit restores the base without enrichment or a write");
  const auto ready = hit && *hit
                         ? (*restarted)->LoadGeometry(key, **hit, view)
                         : std::expected<std::optional<BakedTile>, StructureBakeError>(
                               std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct));
  CHECK(ready && *ready && (**ready).Digest == geometry->Digest &&
            (**ready).Prints == geometry->Prints && (**ready).Coordinates &&
            (**ready).Coordinates->Points == (**hit).PointsLatLon &&
            (*restarted)->Costs().GeometryWrites == 0,
        "restart loads render geometry and owned contacts without meshing or source inputs");
  auto moved = view;
  moved.Eye.LongitudeDeg += 0.01;
  const auto absent = (*restarted)->LoadGeometry(key, **hit, moved);
  CHECK(absent && !*absent, "changed position never reuses a view-dependent LOD snapshot");
  auto changed = shape;
  changed.Gradient = 0.1;
  CHECK((*restarted)->Key(tile, 17, changed, 1000) != key &&
            (*restarted)->Key(tile, 18, shape, 1000) != key &&
            (*restarted)->Key(tile, 17, shape, 2000) != key &&
            (*restarted)->Key(tile, 17, shape, 1000, "new-vector-bytes") != key,
        "terrain shaping, street inputs, payload and scale bind separate base keys");
  const auto changedRequest = (*restarted)->RequestKey(tile, changed, 1000);
  CHECK(changedRequest != request && (*restarted)->RequestKey(tile, shape, 2000) != request &&
            !(*restarted)->LoadBasisRequest(changedRequest)->has_value() &&
            (*restarted)->RequestKey({.Zoom = -1, .X = 0, .Y = 0}, shape, 1000).empty(),
        "native demand separates configuration and rejects invalid cells before any inputs");
  NativeLODWithoutPlans(root, key, *restarted, view, *geometry);
}

void RequestMigration(const std::filesystem::path &root) {
  Data::ContentStore store({.Directory = (root / "sources").string()});
  Data::SourceSet sources(store);
  auto cache = PreparedBuildingAssets::Open(root.string(), sources);
  CHECK(cache.has_value(), "request migration opens");
  if (!cache) { return; }
  const Data::TileId tile{.Zoom = 0, .X = 0, .Y = 0};
  const Ground::ShapedGround shape;
  const auto request = (*cache)->RequestKey(tile, shape, 1000);
  const auto key = (*cache)->Key(tile, 17, shape, 1000);
  const std::atomic_bool stopping{false};
  CHECK((*cache)->Generate(key, Inputs(), *Heights(), stopping).has_value(),
        "legacy producer stores an unbound complete base");
  const auto unbound = (*cache)->LoadBasisRequest(request);
  CHECK(unbound && !*unbound, "unbound products do not claim a request hit");
  const auto before = (*cache)->Costs();
  const auto migrated = (*cache)->LoadBasis(key, request);
  const auto bound = (*cache)->LoadBasisRequest(request);
  CHECK(migrated && *migrated && bound && *bound && (**bound).BaseKey == key &&
            (*cache)->Costs().ReadBytes == before.ReadBytes,
        "migration binds the existing native basis without opening full plans");
  const auto replacement = (*cache)->Key(tile, 18, shape, 1000);
  CHECK((*cache)->Generate(replacement, Inputs(), *Heights(), stopping, request).has_value(),
        "explicit changed input publishes a complete replacement binding");
  const auto current = (*cache)->LoadBasisRequest(request);
  CHECK(current && *current && (**current).BaseKey == replacement && (*cache)->LoadBasis(key),
        "request selects replacement while the previous content remains addressable");
  auto records = AssetCache::Open((root / "assets.sqlite").string());
  CHECK(records.has_value(), "native request metadata fixture opens");
  if (!records) { return; }
  const auto metadata = (*records)->FindRequest(request);
  CHECK(metadata && *metadata, "replacement demand has complete product metadata");
  if (!metadata || !*metadata) { return; }
  auto corrupt = **metadata;
  const auto payload = (*records)->Load(corrupt.Key, size_t{64} * 1024 * 1024);
  CHECK(payload && *payload, "fixture retains native bytes");
  if (!payload || !*payload) { return; }
  corrupt.Kind = "wrong-native-kind";
  CHECK((*records)->Publish(std::span(&corrupt, 1), (**payload).Bytes()).has_value() &&
            !(*cache)->LoadBasisRequest(request)->has_value(),
        "wrong product type is a miss rather than a false native hit");
  const auto repaired = (*cache)->LoadBasis(replacement, request);
  const auto ready = (*cache)->LoadBasisRequest(request);
  CHECK(repaired && *repaired && ready && *ready && (**ready).BaseKey == replacement,
        "cached complete plans repair the request product without source acquisition");
  CHECK((*cache)->InvalidateBasis(replacement).has_value() &&
            !(*cache)->LoadBasisRequest(request)->has_value(),
        "invalidated basis cannot leave a false native request hit");
}

std::optional<PreparedStructureTile> CachedBase(const PreparedStructureTile &base) {
  const auto encoded = EncodePreparedStructureTile(base);
  CHECK(encoded.has_value(), "complete intrinsic forms, roofs and terrain contacts serialize");
  if (!encoded) { return std::nullopt; }
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-prepared-building-cache-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto path = (root / "assets.sqlite").string();
  auto opened = AssetCache::Open(path);
  CHECK(opened.has_value(), "building asset fixture opens the common spatial cache");
  if (!opened) { return std::nullopt; }
  constexpr std::string_view recipe = "prepared-buildings-format-1";
  const auto key = Sha256Hex(recipe.data(), recipe.size());
  size_t generated = 0;
  const auto published = ResolveAsset(**opened, key, encoded->size(), [&](size_t) {
    ++generated;
    GeneratedAssetPackage package;
    package.Bytes = *encoded;
    package.Records.push_back({.Key = key,
                               .Kind = "buildings",
                               .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                               .ByteCount = package.Bytes.size()});
    return std::expected<GeneratedAssetPackage, std::string>(std::move(package));
  });
  CHECK(published && generated == 1, "one miss publishes an actual native prepared building tile");
  opened->reset();
  auto restarted = AssetCache::Open(path);
  CHECK(restarted.has_value(), "fresh cache connection opens the prepared building package");
  if (!restarted) { return std::nullopt; }
  const auto hit = ResolveAsset(**restarted, key, encoded->size(), [&](size_t) {
    ++generated;
    return std::expected<GeneratedAssetPackage, std::string>(std::unexpected("source offline"));
  });
  CHECK(hit && generated == 1, "native package hits bypass the unavailable generator and provider");
  if (!hit) { return std::nullopt; }
  const auto decoded = DecodePreparedStructureTile(hit->Bytes(), 1024 * 1024);
  CHECK(decoded.has_value(), "held cache bytes reconstruct complete source-independent shapes");
  if (!decoded) { return std::nullopt; }
  CHECK(decoded->HeightSources == base.HeightSources &&
            decoded->HeightRasterDigest == base.HeightRasterDigest &&
            decoded->HeightQualified == base.HeightQualified &&
            decoded->HeightRequest.Zoom == base.HeightRequest.Zoom &&
            std::ranges::equal(decoded->HeightRequest.Tiles,
                               base.HeightRequest.Tiles,
                               [](const auto &left, const auto &right) {
                                 return left.Zoom == right.Zoom && left.X == right.X &&
                                        left.Y == right.Y;
                               }) &&
            decoded->HeightRequest.Fallback == base.HeightRequest.Fallback,
        "terrain lineage, sample signature, capture scope and qualification survive restart");
  CHECK(!DecodePreparedStructureTile(hit->Bytes(), 1),
        "native arrays cannot allocate beyond the supplied resident budget");
  auto truncated = *encoded;
  truncated.pop_back();
  CHECK(!DecodePreparedStructureTile(truncated, 1024 * 1024),
        "partial native building packages do not become ready products");
  (*restarted).reset();
  NativeCache(root);
  RequestMigration(root / "request-migration");
  std::filesystem::remove_all(root);
  return decoded;
}

void WorkerBake(const PreparedStructureTile &base,
                const BuildingMesh &mesher,
                const RawTile &view,
                uint64_t reference) {
  Tasks worker(1);
  auto held = std::make_shared<const PreparedStructureTile>(base);
  StructureBuildTask task(0,
                          held,
                          std::make_unique<RawTile>(view),
                          std::make_unique<StructureBuildTask::Output>(),
                          mesher.Scratch());
  held.reset();
  CHECK(task.PreparedBase() != nullptr &&
            std::ranges::equal(task.HeightSources(), base.HeightSources) &&
            task.HeightRasterDigest() == base.HeightRasterDigest &&
            task.HeightQualified() == base.HeightQualified &&
            task.HeightRequest().Tiles.size() == base.HeightRequest.Tiles.size(),
        "worker retains stored terrain lineage without holding a height field");
  task.Start(worker, mesher);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  size_t tasks = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    if (!task.TakeCompletion(worker)) {
      (void)task.AwaitCompletion(0.05);
      continue;
    }
    ++tasks;
    CHECK(task.Result().LastRanges > 0 &&
              task.Result().LastRanges <= StructureBuildTask::RangesPerTask,
          "ready base selection and emission keep the worker task bounded");
    if (!task.Result().Status || task.Result().Tile) { break; }
    task.Resume(worker, mesher);
  }
  if (task.Running()) {
    task.RequestStop();
    task.Join(worker);
  }
  CHECK(task.Result().Status && task.Result().Tile && task.Result().Tile->Digest == reference,
        "source-free worker delivers the same geometry from a held native cache basis");
  CHECK(base.Structures.size() <=
                StructureBuildTask::StructuresPerRange * StructureBuildTask::RangesPerTask ||
            tasks > 1,
        "a large ready tile yields the worker instead of emitting everything in one task");
  CHECK(task.Progress().BakedStructures() == base.Structures.size(),
        "native worker completion counts prepared structures without source progress");
}

void ReadyForms(const PreparedStructureTile &base, const BuildingMesh &mesher) {
  for (size_t index = 0; index < base.Structures.size(); ++index) {
    const auto &record = base.Structures[index];
    const auto corners =
        std::span(base.CornerAslM).subspan(record.CornerFirst, record.Layout.PointCount);
    auto plan =
        PreparedStructurePlan(record, base.PointsLatLon, base.Holes, corners, base.AnchorEcef);
    plan.Prepared = &base.Surfaces[index];
    for (auto detail : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
      plan.Coarseness = detail;
      BuildingScratch scratch;
      Raised mesh;
      CHECK(mesher.SourceEnvelopeBounds(plan, scratch) &&
                mesher.ShellSurfaceErrorM(plan, scratch) && mesher.Mesh(plan, scratch, mesh),
            "prepared native forms supply bounds, error and every mesh envelope");
      CHECK(scratch.Parts.Count() == 0, "ready house forms never repeat intrinsic shape planning");
    }
  }
}

void UnsupportedMass() {
  auto raw = Inputs();
  constexpr double tiny = 0.0000001;
  for (size_t at = 0; at < 4; ++at) {
    raw.LatLon[2 * at] = 47.001 + (at >= 2 ? tiny : 0.0);
    raw.LatLon[2 * at + 1] = 9.001 + (at == 1 || at == 2 ? tiny : 0.0);
  }
  raw.RequestedDetail = LevelOfDetail::Fine;
  const auto heights = Heights();
  const auto base = PrepareStructureTile(raw, *heights);
  CHECK(base && base->Structures.size() == 3 && base->Surfaces.front().Shapes().empty(),
        "sub-resolution footprint retains enriched contacts and explicit unsupported mass");
  if (!base) { return; }
  CHECK(!base->Surfaces.front().SupportsProjection(),
        "unsupported native mass never claims a surface projection");
  const auto encoded = EncodePreparedStructureTile(*base);
  CHECK(encoded.has_value(), "unsupported intrinsic result is a complete cacheable base state");
  if (!encoded) { return; }
  const auto restored = DecodePreparedStructureTile(*encoded, 1024 * 1024);
  CHECK(restored && restored->Surfaces.front().Shapes().empty(),
        "restart retains the unsupported mass without replanning it");
  if (!restored) { return; }
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  BakedTile reference;
  const auto original = BakeStructures(raw, *heights, mesher, *scratch, reference);
  const auto ready = BakePreparedStructures(*restored, raw, mesher, *scratch);
  CHECK(original && ready && ready->Digest == reference.Digest &&
            ready->Prints == reference.Prints &&
            ready->UnsupportedMeshes == reference.UnsupportedMeshes,
        "native unknown shape preserves valid neighbours, footprints and the original mesh "
        "diagnostic");
}

}

int main() {
  UnsupportedMass();
  auto raw = Inputs();
  auto heights = Heights();
  const auto base = PrepareStructureTile(raw, *heights);
  CHECK(base && base->Structures.size() == 3 && base->CornerAslM.size() == 12 &&
            base->Surfaces.size() == 3,
        "one cold pass resolves every source footprint and terrain corner");
  if (!base) { return Report(); }
  CHECK(base->Structures[0].Standing.HeightM > 0 &&
            base->Structures[0].Standing.Source == Ground::BuildingHeightSource::Generated &&
            base->Structures[0].Standing.Street.Known,
        "unknown height and road frontage are fully enriched before runtime selection");
  CHECK(base->Structures[2].Standing.MinimumHeightM == 3.0f,
        "prepared raised parts retain their clearance");
  BuildingMesh mesher;
  std::vector<RawTile> views;
  std::vector<BakedTile> references;
  for (const auto detail : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
    RawTile view;
    view.RequestedDetail = detail;
    view.Eye = {.LongitudeDeg = 10, .LatitudeDeg = 47};
    views.push_back(view);
    auto original = raw;
    original.RequestedDetail = view.RequestedDetail;
    original.Eye = view.Eye;
    auto scratch = mesher.Scratch();
    BakedTile reference;
    CHECK(BakeStructures(original, *heights, mesher, *scratch, reference).has_value(),
          "cold native path supplies the geometry reference at every envelope");
    references.push_back(std::move(reference));
  }
  const auto decoded = CachedBase(*base);
  if (!decoded) { return Report(); }
  raw = {};
  heights.reset();
  ReadyForms(*decoded, mesher);
  WorkerBake(*decoded, mesher, views.front(), references.front().Digest);
  auto batch = *decoded;
  batch.Structures.resize(257, decoded->Structures.front());
  batch.Surfaces.resize(257, decoded->Surfaces.front());
  auto batchScratch = mesher.Scratch();
  const auto batchReference = BakePreparedStructures(batch, views.front(), mesher, *batchScratch);
  CHECK(batchReference.has_value(), "large native tile supplies the bounded worker fixture");
  if (batchReference) { WorkerBake(batch, mesher, views.front(), batchReference->Digest); }
  for (size_t index = 0; index < views.size(); ++index) {
    auto scratch = mesher.Scratch();
    const auto built = BakePreparedStructures(*decoded, views[index], mesher, *scratch);
    CHECK(built && built->Digest == references[index].Digest &&
              built->Prints == references[index].Prints && built->NoGround == 0,
          "prepared bases reproduce cold geometry and contacts after source/height owners vanish");
    CHECK(built && built->Coordinates && built->Coordinates->Points == decoded->PointsLatLon,
          "runtime native footprints carry their own geometry rather than source-cache views");
  }
  std::atomic_bool stopping{true};
  auto scratch = mesher.Scratch();
  const auto cancelled = BakePreparedStructures(*base, views[0], mesher, *scratch, &stopping);
  CHECK(!cancelled && cancelled.error() == StructureBakeError(StructureBakeErrorKind::Cancelled),
        "prepared native generation still obeys cancellation");
  return Report();
}
