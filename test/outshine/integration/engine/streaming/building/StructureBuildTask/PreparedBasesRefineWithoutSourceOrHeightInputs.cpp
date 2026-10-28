#include <span>
#include <string_view>
#include "PreparedStructureTile.h"
#include "PreparedBuildingAssets.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "StructureBuildTask.h"
#include "StructureArtifact.h"
#include "Tasks.h"
#include "PreparedStructureCodec.h"
#include "PreparedStructurePlan.h"
#include "AssetGeneration.h"
#include "Sha256.h"
#include "BuildingScratch.h"
#include "BuildingMesh.h"
#include "Check.h"
#include "Geodesy.h"
#include "Units.h"
#include <array>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <chrono>
#include <string>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <variant>
#include <sqlite3.h>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr std::string_view kInputDigest = "prepared-native-building-fixture-input";

RawTile Inputs() {
  RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47}, raw.AnchorEcef);
  const Ground::GeoBounds region{
      .MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.02, .MaxLatDeg = 47.02};
  constexpr std::array facades{
      FacadeStyle::Commercial, FacadeStyle::Institutional, FacadeStyle::Religious};
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
                              .Pitched = 0,
                              .WallColour = {},
                              .Facade = facades[index]});
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
  const BuildingMesh mesher;
  Tasks pool(1);
  StructureBuildTask task(0,
                          *basis,
                          std::make_unique<RawTile>(view),
                          std::make_unique<StructureBuildTask::Output>(),
                          mesher.Scratch());
  task.UseNativeAssets(cache, key);
  task.Start(pool, mesher);
  task.Join(pool);
  const auto &loaded = task.Result();
  CHECK(loaded.Status && loaded.Tile && loaded.CacheHit &&
            loaded.Tile->Digest == reference.Digest && loaded.Tile->Prints == reference.Prints &&
            std::ranges::equal(loaded.Tile->Coordinates->Points(), (*basis)->PointsLatLon) &&
            task.PreparedBase() == nullptr && cache->Costs().ReadBytes == before.ReadBytes,
        "a cached LOD and its owned contacts load with no full plan product or source inputs");
  RawTile moved = view;
  moved.Eye.LongitudeDeg += 0.01;
  StructureBuildTask replay(0,
                            *basis,
                            std::make_unique<RawTile>(moved),
                            std::make_unique<StructureBuildTask::Output>(),
                            mesher.Scratch());
  replay.UseNativeAssets(cache, key);
  replay.Start(pool, mesher);
  replay.Join(pool);
  const auto &reloaded = replay.Result();
  CHECK(reloaded.Status && reloaded.Tile && reloaded.CacheHit &&
            reloaded.Tile->Digest == reference.Digest && replay.PreparedBase() == nullptr &&
            cache->Costs().ReadBytes == before.ReadBytes,
        "moved camera loads an existing intrinsic LOD without reopening plans or source inputs");
  moved.ClusterTriangles = 64;
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

std::optional<Vec3> FarthestPoint(const Vec3 &anchor, const Raised &mesh) {
  std::optional<Vec3> remotePoint;
  double most = -1;
  const auto farthest = [&](const auto &vertices) {
    for (const auto &vertex : vertices) {
      const Vec3 local{{vertex.pos[0], vertex.pos[1], vertex.pos[2]}};
      if (Dot(local, local) > most) {
        most = Dot(local, local);
        remotePoint = anchor + local;
      }
    }
  };
  farthest(mesh.WallCorners);
  farthest(mesh.RoofCorners);
  return remotePoint;
}

void CheckStoredLod(PreparedBuildingAssets &cache,
                    const std::string &key,
                    const PreparedStructureTile &base,
                    const RawTile &view,
                    std::string_view identity,
                    std::span<const uint8_t> expected,
                    uint64_t writes) {
  const auto loaded = cache.LoadGeometry(key, base, view);
  const auto bytes = loaded && *loaded
                         ? EncodeStructureArtifact(**loaded, identity)
                         : std::expected<std::vector<uint8_t>, StructureArtifactError>(
                               std::unexpected(StructureArtifactError::InvalidProduct));
  CHECK(bytes && std::ranges::equal(*bytes, expected) && cache.Costs().GeometryWrites == writes + 1,
        "another camera loads the exact intrinsic LOD without another product write");
  auto layout = view;
  layout.ClusterTriangles = 64;
  const auto distinct = cache.LoadGeometry(key, base, layout);
  CHECK(distinct && !*distinct, "cluster layout remains a distinct prepared product");
}

void IntrinsicLODs(const std::filesystem::path &root,
                   PreparedBuildingAssets &cache,
                   const std::string &key,
                   const PreparedStructureTile &base) {
  const BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  std::optional<Vec3> remotePoint;
  for (const bool cell : {false, true}) {
    for (const auto detail : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
      RawTile near;
      near.RequestedDetail = detail;
      if (cell) { near.RequestedCell = base.Structures.front().Layout.Cell.Index; }
      near.Eye = {.LongitudeDeg = 9.001, .LatitudeDeg = 47.001};
      Vec3 eye;
      GeoToEcef({.LongitudeDeg = near.Eye.LongitudeDeg, .LatitudeDeg = near.Eye.LatitudeDeg}, eye);
      near.EyeEcef = eye;
      near.Projection = {.FocalPx = 720, .AllowedErrorPx = 1};
      near.ClusterTriangles = 128;
      auto far = near;
      far.Eye.LongitudeDeg += 2;
      GeoToEcef({.LongitudeDeg = far.Eye.LongitudeDeg, .LatitudeDeg = far.Eye.LatitudeDeg}, eye);
      far.EyeEcef = eye;
      far.Projection = {.FocalPx = 1080, .AllowedErrorPx = 0.25};
      const auto original = BakePreparedStructures(base, near, mesher, *scratch);
      const auto remote = BakePreparedStructures(base, far, mesher, *scratch);
      CHECK(original && remote, "each explicit whole/cell LOD is constructible at both views");
      if (!original || !remote) { continue; }
      if (!cell && detail == LevelOfDetail::Fine) {
        remotePoint = FarthestPoint(base.AnchorEcef, original->Built);
      }
      const std::string identity(64, 'a');
      const auto a = EncodeStructureArtifact(*original, identity);
      const auto b = EncodeStructureArtifact(*remote, identity);
      CHECK(a && b && *a == *b && !original->SelectedView && !remote->SelectedView,
            "explicit LOD payload and contacts are independent of eye and pixel projection");
      const auto writes = cache.Costs().GeometryWrites;
      CHECK(cache.StoreGeometry(key, base, near, *original).has_value(),
            "one intrinsic LOD is published for its source, cell and layout");
      if (a) { CheckStoredLod(cache, key, base, far, identity, *a, writes); }
    }
  }
  auto index = AssetCache::Open((root / "assets.sqlite").string());
  CHECK(index && remotePoint, "static LODs have indexed geometry in the source world frame");
  if (index && remotePoint) {
    const auto selected = (*index)->Select({.Bounds = {.Min = *remotePoint, .Max = *remotePoint},
                                            .Kind = "building-static-lod",
                                            .Level = static_cast<uint32_t>(LevelOfDetail::Fine),
                                            .Radius = {},
                                            .Planes = {}});
    CHECK(selected && selected->size() == 1 && selected->front().Parent == key,
          "a remote visible point selects whole Fine and excludes the unrelated Fine cell");
  }
  RawTile adaptive;
  adaptive.Eye = {.LongitudeDeg = 9.001, .LatitudeDeg = 47.001};
  adaptive.Projection = {.FocalPx = 720, .AllowedErrorPx = 1};
  const auto selected = BakePreparedStructures(base, adaptive, mesher, *scratch);
  CHECK(selected && selected->SelectedView &&
            cache.StoreGeometry(key, base, adaptive, *selected).has_value(),
        "automatic view selection remains an explicitly scoped product");
  adaptive.Eye.LongitudeDeg += 0.01;
  const auto moved = cache.LoadGeometry(key, base, adaptive);
  CHECK(moved && !*moved, "automatic view-dependent snapshots cannot alias intrinsic LODs");
}

void NativeCache(const std::filesystem::path &root) {
  Data::RawSourceCache store({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(store);
  const auto directory = root.string();
  auto opened = PreparedBuildingAssets::Open(directory, sources);
  CHECK(opened.has_value(), "native building service opens the shared spatial cache");
  if (!opened) { return; }
  const Data::TileId tile{.Zoom = 0, .X = 0, .Y = 0};
  const Ground::ShapedGround shape;
  const auto request = (*opened)->RequestKey(tile, shape, 1000, kInputDigest);
  const auto key = (*opened)->Key(tile, 17, shape, 1000, kInputDigest);
  const auto miss = (*opened)->Load(key);
  CHECK(miss && !*miss, "native base miss is distinct from storage failure");
  const std::atomic_bool stopping{false};
  const auto generated = (*opened)->Generate(key, Inputs(), *Heights(), stopping, request);
  CHECK(generated && *generated && (*generated)->Structures.size() == 3,
        "cache miss publishes and reloads complete intrinsic building assets");
  const BuildingMesh mesher;
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
            std::ranges::equal((**ready).Coordinates->Points(), (**hit).PointsLatLon) &&
            (*restarted)->Costs().GeometryWrites == 0,
        "restart loads render geometry and owned contacts without meshing or source inputs");
  auto moved = view;
  moved.Eye.LongitudeDeg += 0.01;
  const auto movedGeometry = (*restarted)->LoadGeometry(key, **hit, moved);
  CHECK(movedGeometry && *movedGeometry && (**movedGeometry).Digest == geometry->Digest,
        "explicit Shell is the same intrinsic asset after camera movement");
  auto changed = shape;
  changed.Gradient = 0.1;
  CHECK((*restarted)->Key(tile, 17, changed, 1000, kInputDigest) != key &&
            (*restarted)->Key(tile, 18, shape, 1000, kInputDigest) != key &&
            (*restarted)->Key(tile, 17, shape, 2000, kInputDigest) != key &&
            (*restarted)->Key(tile, 17, shape, 1000, "new-vector-bytes") != key,
        "terrain shaping, street inputs, payload and scale bind separate base keys");
  const auto changedRequest = (*restarted)->RequestKey(tile, changed, 1000, kInputDigest);
  CHECK(
      changedRequest != request &&
          (*restarted)->RequestKey(tile, shape, 2000, kInputDigest) != request &&
          !(*restarted)->LoadBasisRequest(changedRequest)->has_value() &&
          (*restarted)->RequestKey({.Zoom = -1, .X = 0, .Y = 0}, shape, 1000, kInputDigest).empty(),
      "native demand separates configuration and rejects invalid cells before any inputs");
  if (hit && *hit) { IntrinsicLODs(root, **restarted, key, **hit); }
  NativeLODWithoutPlans(root, key, *restarted, view, *geometry);
}

void RequestMigration(const std::filesystem::path &root) {
  Data::RawSourceCache store({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(store);
  auto cache = PreparedBuildingAssets::Open(root.string(), sources);
  CHECK(cache.has_value(), "request migration opens");
  if (!cache) { return; }
  const Data::TileId tile{.Zoom = 0, .X = 0, .Y = 0};
  const Ground::ShapedGround shape;
  const auto request = (*cache)->RequestKey(tile, shape, 1000, kInputDigest);
  const auto key = (*cache)->Key(tile, 17, shape, 1000, kInputDigest);
  const std::atomic_bool stopping{false};
  CHECK((*cache)->Generate(key, Inputs(), *Heights(), stopping).has_value(),
        "legacy producer stores an unbound complete base");
  const auto unbound = (*cache)->LoadBasisRequest(request);
  CHECK(unbound && !*unbound, "unbound products do not claim a request hit");
  sqlite3 *raw = nullptr;
  const int opened = sqlite3_open((root / "assets.sqlite").string().c_str(), &raw);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, &sqlite3_close);
  CHECK(opened == SQLITE_OK, "request binding write guard opens the existing cache");
  if (opened != SQLITE_OK) { return; }
  constexpr const char *protect =
      "CREATE TRIGGER binding_no_insert BEFORE INSERT ON packages "
      "BEGIN SELECT RAISE(ABORT,'package insert during metadata binding'); END;"
      "CREATE TRIGGER binding_no_update BEFORE UPDATE ON packages "
      "BEGIN SELECT RAISE(ABORT,'package update during metadata binding'); END;"
      "CREATE TRIGGER binding_no_delete BEFORE DELETE ON packages "
      "BEGIN SELECT RAISE(ABORT,'package delete during metadata binding'); END;";
  const int protectedPackages = sqlite3_exec(database.get(), protect, nullptr, nullptr, nullptr);
  CHECK(protectedPackages == SQLITE_OK, "request migration forbids all native package writes");
  if (protectedPackages != SQLITE_OK) { return; }
  const auto original = (*cache)->LoadBasis(key);
  const auto before = (*cache)->Costs();
  const auto migrated = (*cache)->LoadBasis(key, request);
  const auto bound = (*cache)->LoadBasisRequest(request);
  CHECK(migrated && *migrated && bound && *bound && (**bound).BaseKey == key &&
            (*cache)->Costs().ReadBytes == before.ReadBytes,
        "migration binds the existing native basis without opening full plans");
  CHECK(original && *original && migrated && *migrated &&
            EncodePreparedBuildingBasis(**original) == EncodePreparedBuildingBasis(**migrated) &&
            (*cache)->Costs().BasisWrites == before.BasisWrites &&
            (*cache)->Costs().Writes == before.Writes &&
            (*cache)->Costs().SurfaceWrites == before.SurfaceWrites &&
            (*cache)->Costs().GeometryWrites == before.GeometryWrites,
        "binding preserves the complete basis and never counts a native product write");
  constexpr const char *unprotect = "DROP TRIGGER binding_no_insert;"
                                    "DROP TRIGGER binding_no_update;"
                                    "DROP TRIGGER binding_no_delete;";
  const int writable = sqlite3_exec(database.get(), unprotect, nullptr, nullptr, nullptr);
  CHECK(writable == SQLITE_OK, "later replacement and repair retain normal package publication");
  if (writable != SQLITE_OK) { return; }
  const auto replacement = (*cache)->Key(tile, 18, shape, 1000, kInputDigest);
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

bool CheckResidentMetadata(PreparedBuildingAssets &cache,
                           const std::filesystem::path &root,
                           const std::string &request,
                           const std::weak_ptr<const PreparedBuildingBasis> &weak) {
  const auto original = weak.lock();
  CHECK(original, "live geometry still owns the basis during metadata replacement");
  if (!original) { return false; }
  const auto *identity = original.get();
  auto records = AssetCache::Open((root / "assets.sqlite").string());
  CHECK(records.has_value(), "resident fixture opens current request metadata");
  if (!records) { return false; }
  const auto record = (*records)->FindRequest(request);
  CHECK(record && *record, "live basis has a current native asset record");
  if (!record || !*record) { return false; }
  const auto payload = (*records)->Load((**record).Key, size_t{64} * 1024 * 1024);
  CHECK(payload && *payload, "resident metadata fixture retains the original native packet");
  if (!payload || !*payload) { return false; }
  auto corrupt = **record;
  corrupt.Kind = "wrong-native-kind";
  CHECK((*records)->Publish(std::span(&corrupt, 1), (**payload).Bytes()).has_value(),
        "resident fixture changes only current metadata");
  const auto rejected = cache.LoadBasisRequest(request);
  CHECK(rejected && !*rejected && !weak.expired(),
        "a live old product cannot mask the wrong current record kind");
  CHECK((*records)->Publish(std::span(&**record, 1), (**payload).Bytes()).has_value(),
        "resident fixture restores the original verified record");
  {
    const auto restored = cache.LoadBasisRequest(request);
    CHECK(restored && *restored && (**restored).Product.get() == identity,
          "restored identical metadata permits the still-live verified product");
  }
  auto changed = *original;
  changed.HeightRasterDigest = changed.HeightRasterDigest == 1 ? 2 : 1;
  const auto changedBytes = EncodePreparedBuildingBasis(changed);
  CHECK(changedBytes.has_value(), "replacement packet retains the native basis contract");
  if (!changedBytes) { return false; }
  auto changedRecord = **record;
  changedRecord.Package.clear();
  changedRecord.ByteCount = changedBytes->size();
  CHECK((*records)->Publish(std::span(&changedRecord, 1), *changedBytes).has_value(),
        "a repaired packet replaces the same indexed product identity");
  {
    const auto repaired = cache.LoadBasisRequest(request);
    CHECK(repaired && *repaired && (**repaired).Product.get() != identity &&
              (**repaired).Product->HeightRasterDigest == changed.HeightRasterDigest,
          "a changed package cannot reuse a live previous packet under the same asset key");
  }
  CHECK((*records)->Publish(std::span(&**record, 1), (**payload).Bytes()).has_value(),
        "the fixture restores original bytes before explicit invalidation");
  return true;
}

void ResidentRequests(const std::filesystem::path &root) {
  Data::RawSourceCache store({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(store);
  auto cache = PreparedBuildingAssets::Open(root.string(), sources);
  CHECK(cache.has_value(), "resident basis fixture opens");
  if (!cache) { return; }
  const Data::TileId tile{.Zoom = 0, .X = 0, .Y = 0};
  const Ground::ShapedGround shape;
  const auto request = (*cache)->RequestKey(tile, shape, 1000, kInputDigest);
  const auto key = (*cache)->Key(tile, 17, shape, 1000, kInputDigest);
  const std::atomic_bool stopping{false};
  const auto generated = (*cache)->Generate(key, Inputs(), *Heights(), stopping, request);
  CHECK(generated && *generated, "resident fixture publishes real complete building inputs");
  if (!generated || !*generated) { return; }
  const BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  RawTile view;
  view.RequestedDetail = LevelOfDetail::Shell;
  view.Eye = {.LongitudeDeg = 10, .LatitudeDeg = 47};
  const auto built = BakePreparedStructures(**generated, view, mesher, *scratch);
  CHECK(built && (*cache)->StoreGeometry(key, **generated, view, *built).has_value(),
        "resident fixture stores the actual intrinsic geometry consumer");
  if (!built) { return; }
  auto first = (*cache)->LoadBasisRequest(request);
  CHECK(first && *first, "initial request decodes a complete native basis");
  if (!first || !*first) { return; }
  const auto *identity = (**first).Product.get();
  const std::weak_ptr<const PreparedBuildingBasis> weak = (**first).Product;
  const auto encoded = EncodePreparedBuildingBasis(*(**first).Product);
  auto geometry = (*cache)->LoadGeometry(key, (**first).Product, view);
  CHECK(encoded && geometry && *geometry && (**geometry).Coordinates,
        "the native geometry consumer retains the basis coordinates");
  if (!encoded || !geometry || !*geometry) { return; }
  first->reset();
  CHECK(!weak.expired(), "live geometry keeps its backing basis alive");
  if (weak.expired()) { return; }
  const auto before = (*cache)->Costs();
  for (size_t repeat = 0; repeat < 8; ++repeat) {
    const auto again = (*cache)->LoadBasisRequest(request);
    CHECK(again && *again && (**again).Product.get() == identity &&
              EncodePreparedBuildingBasis(*(**again).Product) == encoded,
          "repeated requests share the exact verified native basis and byte content");
  }
  const auto reused = (*cache)->Costs();
  CHECK(reused.BasisResidentHits == before.BasisResidentHits + 8 &&
            reused.BasisHits == before.BasisHits &&
            reused.BasisReadBytes == before.BasisReadBytes && reused.ReadBytes == before.ReadBytes,
        "resident reuse performs no native payload or complete-plan reads");
  if (!CheckResidentMetadata(**cache, root, request, weak)) { return; }
  CHECK((*cache)->InvalidateBasis(key).has_value(), "explicit native invalidation succeeds");
  const auto invalidated = (*cache)->LoadBasisRequest(request);
  CHECK(invalidated && !*invalidated && !weak.expired(),
        "explicit invalidation remains a miss while old geometry still owns its basis");
  const auto replacementKey = (*cache)->Key(tile, 18, shape, 1000, kInputDigest);
  CHECK((*cache)->Generate(replacementKey, Inputs(), *Heights(), stopping, request).has_value(),
        "changed physical input publishes a complete replacement binding");
  auto replacement = (*cache)->LoadBasisRequest(request);
  CHECK(replacement && *replacement && (**replacement).BaseKey == replacementKey &&
            (**replacement).Product.get() != identity,
        "current request replacement cannot reuse a previous live content identity");
  if (!replacement || !*replacement) { return; }
  const std::weak_ptr<const PreparedBuildingBasis> replacementWeak = (**replacement).Product;
  replacement->reset();
  geometry->reset();
  CHECK(weak.expired() && replacementWeak.expired(),
        "the resident index retains no basis payload after its real consumers disappear");
  const auto expired = (*cache)->Costs();
  const auto reloaded = (*cache)->LoadBasisRequest(request);
  CHECK(reloaded && *reloaded && (*cache)->Costs().BasisHits == expired.BasisHits + 1 &&
            (*cache)->Costs().BasisResidentHits == expired.BasisResidentHits,
        "expired weak products follow the real payload-read path again");
}

void ResidentRequestLimit(const std::filesystem::path &root) {
  Data::RawSourceCache store({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(store);
  auto cache = PreparedBuildingAssets::Open(root.string(), sources);
  CHECK(cache.has_value(), "bounded resident fixture opens");
  if (!cache) { return; }
  const Data::TileId tile{.Zoom = 0, .X = 0, .Y = 0};
  const Ground::ShapedGround shape;
  const auto request = (*cache)->RequestKey(tile, shape, 1000, kInputDigest);
  const auto key = (*cache)->Key(tile, 17, shape, 1000, kInputDigest);
  const std::atomic_bool stopping{false};
  CHECK((*cache)->Generate(key, Inputs(), *Heights(), stopping, request).has_value(),
        "bounded fixture publishes one real native basis");
  auto first = (*cache)->LoadBasisRequest(request);
  auto records = AssetCache::Open((root / "assets.sqlite").string());
  CHECK(first && *first && records, "bounded fixture opens its current product and metadata");
  if (!first || !*first || !records) { return; }
  const auto encoded = EncodePreparedBuildingBasis(*(**first).Product);
  const auto record = (*records)->FindRequest(request);
  CHECK(encoded && record && *record, "bounded fixture has one complete verified packet");
  if (!encoded || !record || !*record) { return; }
  std::vector<AssetRecord> declarations;
  std::vector<std::string> requests{request};
  for (size_t index = 0; index < 256; ++index) {
    auto sibling = **record;
    sibling.Parent = Sha256Hex("resident-parent-" + std::to_string(index));
    sibling.Key = Sha256Hex("prepared-building-basis-2/" + sibling.Parent);
    sibling.RequestKey = Sha256Hex("resident-request-" + std::to_string(index));
    sibling.Package.clear();
    sibling.OffsetBytes = 0;
    sibling.ByteCount = encoded->size();
    requests.push_back(*sibling.RequestKey);
    declarations.push_back(std::move(sibling));
  }
  CHECK((*records)->Publish(declarations, *encoded).has_value(),
        "bounded fixture publishes independent identities sharing one valid packet");
  std::vector<PreparedBuildingAssets::Basis> held{(**first).Product};
  first->reset();
  for (size_t index = 1; index < requests.size(); ++index) {
    const auto loaded = (*cache)->LoadBasisRequest(requests[index]);
    CHECK(loaded && *loaded, "each independently requested basis remains loadable");
    if (!loaded || !*loaded) { return; }
    held.push_back((**loaded).Product);
  }
  const auto full = (*cache)->Costs();
  for (size_t index = 0; index < 256; ++index) {
    const auto reused = (*cache)->LoadBasisRequest(requests[index]);
    CHECK(reused && *reused && (**reused).Product == held[index],
          "the bounded index preserves each remembered live product");
  }
  CHECK((*cache)->Costs().BasisResidentHits == full.BasisResidentHits + 256 &&
            (*cache)->Costs().BasisHits == full.BasisHits,
        "all remembered products reuse their payloads without additional reads");
  const auto beforeOverflow = (*cache)->Costs();
  {
    const auto overflow = (*cache)->LoadBasisRequest(requests.back());
    CHECK(overflow && *overflow && (*cache)->Costs().BasisHits == beforeOverflow.BasisHits + 1 &&
              (*cache)->Costs().BasisResidentHits == beforeOverflow.BasisResidentHits,
          "a full index does not grow to retain another simultaneously live identity");
  }
  const auto survivor = held.front();
  const std::weak_ptr<const PreparedBuildingBasis> expired = held[1];
  held.clear();
  CHECK(expired.expired(), "expired slots retain no native payloads");
  const auto recovered = (*cache)->LoadBasisRequest(requests.back());
  const auto beforeReuse = (*cache)->Costs();
  const auto reused = (*cache)->LoadBasisRequest(requests.back());
  CHECK(recovered && *recovered && reused && *reused &&
            (**reused).Product == (**recovered).Product &&
            (*cache)->Costs().BasisResidentHits == beforeReuse.BasisResidentHits + 1 &&
            (*cache)->Costs().BasisHits == beforeReuse.BasisHits,
        "expired metadata slots admit and reuse a previously unremembered product");
  const auto preserved = (*cache)->LoadBasisRequest(requests.front());
  CHECK(preserved && *preserved && (**preserved).Product == survivor,
        "removing expired entries preserves the remaining live consumer");
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
                               .Package = {},
                               .ByteCount = package.Bytes.size(),
                               .Parent = {}});
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
  auto decoded = DecodePreparedStructureTile(hit->Bytes(), static_cast<long>(1024) * 1024);
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
  CHECK(!DecodePreparedStructureTile(truncated, size_t{1024} * 1024),
        "partial native building packages do not become ready products");
  (*restarted).reset();
  NativeCache(root);
  RequestMigration(root / "request-migration");
  ResidentRequests(root / "resident-requests");
  ResidentRequestLimit(root / "resident-limit");
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
  const auto &completed = task.Result();
  CHECK(completed.Status && completed.Tile && completed.Tile->Digest == reference,
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
    for (const auto detail : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
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

void AttachPreparedContacts(BakedTile &reference, const PreparedStructureTile &base) {
  CHECK(reference.Prints.size() == base.Structures.size(),
        "legacy cold footprints retain every prepared source");
  if (reference.Prints.size() != base.Structures.size()) { return; }
  for (size_t index = 0; index < reference.Prints.size(); ++index) {
    auto &print = reference.Prints[index];
    CHECK(!print.TerrainCeiling && print.FirstPoint == base.Structures[index].Standing.FirstPoint,
          "legacy raw meshing has no asset-cook contact metadata and retains source order");
    print.TerrainCeiling = base.Structures[index].Standing.TerrainCeiling;
    print.TerrainContacts = base.Structures[index].Standing.TerrainContacts;
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
  const auto restored = DecodePreparedStructureTile(*encoded, static_cast<long>(1024) * 1024);
  CHECK(restored && restored->Surfaces.front().Shapes().empty(),
        "restart retains the unsupported mass without replanning it");
  if (!restored) { return; }
  const BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  BakedTile reference;
  const auto original = BakeStructures(raw, *heights, mesher, *scratch, reference);
  const auto ready = BakePreparedStructures(*restored, raw, mesher, *scratch);
  AttachPreparedContacts(reference, *base);
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
  const BuildingMesh mesher;
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
    AttachPreparedContacts(reference, *base);
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
    CHECK(built && built->Coordinates &&
              std::ranges::equal(built->Coordinates->Points(), decoded->PointsLatLon),
          "runtime native footprints carry their own geometry rather than source-cache views");
  }
  const std::atomic_bool stopping{true};
  auto scratch = mesher.Scratch();
  const auto cancelled = BakePreparedStructures(*base, views[0], mesher, *scratch, &stopping);
  CHECK(!cancelled && cancelled.error() == StructureBakeError(StructureBakeErrorKind::Cancelled),
        "prepared native generation still obeys cancellation");
  return Report();
}
