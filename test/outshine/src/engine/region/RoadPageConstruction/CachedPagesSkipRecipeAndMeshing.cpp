#include "ControlledRoadPageProducer.h"
#include "RoadPageConstruction.h"
#include "RoadPageRestoration.h"
#include "GroundRegionPreparation.h"
#include "GroundRegionAsset.h"
#include "WaterAsset.h"
#include "ProfiledRoadMesher.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <limits>
#include <stop_token>
#include <sqlite3.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <print>
#include <utility>
#include "Vec3.h"
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

bool Prepare(RoadPageConstruction &build, const Corridors::Job &plan, const Corridors::Site &site) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  while (std::chrono::steady_clock::now() < until) {
    const auto ready = build.Advance(plan, site);
    if (!ready) { return false; }
    if (*ready) { return true; }
    std::this_thread::yield();
  }
  return false;
}

bool Complete(RoadPageRestoration &restore, Geometry &into) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  while (std::chrono::steady_clock::now() < until) {
    const auto ready = restore.Advance(into);
    if (!ready || restore.Missing()) { return false; }
    if (*ready) { return true; }
    std::this_thread::yield();
  }
  return false;
}

std::optional<GroundRegionPreparation::Completed>
LoadRegion(Tasks &pool,
           const std::shared_ptr<PreparedRoadPages> &pages,
           const std::vector<uint8_t> &bytes) {
  GroundRegionPreparation worker(
      pool,
      [&bytes](const std::stop_token &)
          -> std::expected<GroundRegionPreparation::Completed, std::string> {
        auto region = DecodeGroundRegionAsset(bytes, size_t{1024} * 1024);
        if (!region) { return std::unexpected("invalid native region fixture"); }
        return GroundRegionPreparation::Completed{
            .Key = std::string(64, 'a'),
            .Loaded = Osm::PreparedGroundRegions::Loaded{.Region = std::move(*region),
                                                         .Water = {},
                                                         .Key = {}},
            .RequestKey = {},
            .SurfacePages = nullptr};
      },
      pages);
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  while (std::chrono::steady_clock::now() < until) {
    auto result = worker.Collect();
    if (result) {
      if (!*result) { return std::nullopt; }
      return std::move(**result);
    }
    std::this_thread::yield();
  }
  return std::nullopt;
}

bool SameMesh(const Geometry &parts, const Geometry &whole) {
  if (whole.parts() != 1 || !parts.wellFormed()) { return false; }
  const auto same = [](std::span<const float> a, std::span<const float> b) {
    return std::ranges::equal(std::as_bytes(a), std::as_bytes(b));
  };
  size_t vertices = 0;
  size_t triangles = 0;
  for (int part = 0; part < parts.parts(); ++part) {
    const auto positions = parts.positionsOf(part);
    const auto normals = parts.normalsOf(part);
    const auto colours = parts.coloursOf(part);
    const auto indices = parts.trianglesOf(part);
    if (3 * vertices + positions.size() > whole.positionsOf(0).size() ||
        4 * vertices + colours.size() > whole.coloursOf(0).size() ||
        triangles + indices.size() > whole.trianglesOf(0).size() ||
        !same(positions, whole.positionsOf(0).subspan(3 * vertices, positions.size())) ||
        !same(normals, whole.normalsOf(0).subspan(3 * vertices, normals.size())) ||
        !same(colours, whole.coloursOf(0).subspan(4 * vertices, colours.size()))) {
      return false;
    }
    for (size_t at = 0; at < indices.size(); ++at) {
      if (indices[at] + vertices != whole.trianglesOf(0)[triangles + at]) { return false; }
    }
    if (!(parts.surfaceAt(parts.materialOf(part)) == whole.surfaceAt(whole.materialOf(0)))) {
      return false;
    }
    vertices += positions.size() / 3;
    triangles += indices.size();
  }
  return vertices * 3 == whole.positionsOf(0).size() && triangles == whole.trianglesOf(0).size();
}

bool ChangeRecord(const std::filesystem::path &directory, const std::string &key, const char *sql) {
  sqlite3 *raw = nullptr;
  if (sqlite3_open_v2(
          (directory / "assets.sqlite").c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) !=
      SQLITE_OK) {
    if (raw != nullptr) { (void)sqlite3_close(raw); }
    return false;
  }
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, &sqlite3_close);
  sqlite3_stmt *statement = nullptr;
  if (sqlite3_prepare_v2(raw, sql, -1, &statement, nullptr) != SQLITE_OK) { return false; }
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> update(statement,
                                                                          &sqlite3_finalize);
  return sqlite3_bind_text(
             statement, 1, key.c_str(), static_cast<int>(key.size()), SQLITE_TRANSIENT) ==
             SQLITE_OK &&
         sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(raw) == 1;
}

void CheckSelection(Tasks &pool,
                    const std::shared_ptr<PreparedRoadPages> &pages,
                    const std::shared_ptr<const RoadPageManifest> &manifest,
                    const std::string &key,
                    const std::filesystem::path &directory,
                    const std::vector<uint8_t> &region) {
  using namespace outshine::Test;
  CHECK(manifest->Pages.size() >= 2,
        "the selection fixture has multiple spatial native page packages");
  if (manifest->Pages.size() < 2) { return; }
  const auto retained = pages->LoadKey(manifest->Pages[1].Key, size_t{64} * size_t{1024} * 1024);
  CHECK(retained && *retained, "the original page is retained for exact repair after corruption");
  if (!retained || !*retained) { return; }
  const auto address = (**retained).Page->Address;
  CHECK(ChangeRecord(
            directory,
            manifest->Pages[1].Key,
            "UPDATE packages SET crc=crc+1 WHERE key=(SELECT package FROM assets WHERE key=?1)"),
        "the excluded page payload is deliberately unreadable");
  const auto freshMetadata = pages->LoadManifest(key);
  CHECK(freshMetadata && *freshMetadata,
        "manifest restoration never reads an excluded fine payload");
  const auto unverified = pages->Inspect(address, size_t{64} * size_t{1024} * 1024);
  CHECK(unverified && *unverified,
        "an index probe defers payload integrity until that fine page is actually required");
  const std::array<uint32_t, 1> selected{0};
  auto one =
      RoadPageRestoration::Begin(pool, pages, manifest, std::span<const uint32_t>(selected), key);
  Geometry visible;
  CHECK(one && Complete(**one, visible) && (*one)->Pages() == 1 &&
            (*one)->NativeBytes() == manifest->Pages[0].ByteCount && visible.parts() == 1,
        "only the selected independent fine page is read and adopted");
  const std::array<uint32_t, 2> duplicate{0, 0};
  CHECK(!RoadPageRestoration::Begin(pool, pages, manifest, std::span<const uint32_t>(duplicate)),
        "duplicate page selection cannot duplicate world geometry");
  const std::array<uint32_t, 1> outside{static_cast<uint32_t>(manifest->Pages.size())};
  CHECK(!RoadPageRestoration::Begin(pool, pages, manifest, std::span<const uint32_t>(outside)),
        "unknown pages are not silently omitted");
  auto all = RoadPageRestoration::Begin(pool, pages, manifest, std::nullopt, key);
  Geometry incomplete;
  CHECK(all && !Complete(**all, incomplete) && (*all)->Missing(),
        "a required corrupt payload produces a miss instead of a complete partial world");
  const auto invalidated = pages->LoadManifest(key);
  CHECK(invalidated && !*invalidated,
        "the miss worker invalidates its incomplete manifest before returning");
  const auto retry = LoadRegion(pool, pages, region);
  CHECK(retry && !retry->Loaded && !retry->SurfacePages,
        "the regular region worker returns a normal miss for bounded regeneration");
  const auto removed = pages->Inspect(address, size_t{64} * size_t{1024} * 1024);
  CHECK(removed && !*removed,
        "the failed payload loses its index row so metadata hits cannot cause a rebuild loop");
  size_t repairs = 0;
  const auto repaired = pages->Resolve(
      {.Address = address,
       .Parent = {},
       .RequestKey = {},
       .NativeBytesMost = size_t{64} * size_t{1024} * 1024},
      outshine::Test::RoadProducer([&](const RoadPageRequest &, const std::stop_token &) {
        ++repairs;
        return std::expected<RoadPageGeometry, std::string>((**retained).Page->Geometry);
      }).get());
  CHECK(repaired && repairs == 1, "exactly one generation repairs the corrupt native package");
  const auto published = pages->StoreManifest(*manifest);
  auto complete = RoadPageRestoration::Begin(pool, pages, manifest, std::nullopt, key);
  Geometry rebuilt;
  CHECK(published && *published == key && complete && Complete(**complete, rebuilt) &&
            (*complete)->Pages() == manifest->Pages.size(),
        "the repaired product can republish the same manifest and complete the full selection");
  CHECK(ChangeRecord(
            directory, manifest->Pages[1].Key, "UPDATE assets SET level=level+1 WHERE key=?1"),
        "the indexed level is deliberately changed while the original payload stays valid");
  const auto wrongLevel = pages->Inspect(address, size_t{64} * size_t{1024} * 1024);
  CHECK(wrongLevel && !*wrongLevel,
        "inconsistent index metadata becomes a regenerable miss before any fine read");
  const auto corrected = pages->Resolve(
      {.Address = address,
       .Parent = {},
       .RequestKey = {},
       .NativeBytesMost = size_t{64} * size_t{1024} * 1024},
      outshine::Test::RoadProducer([&](const RoadPageRequest &, const std::stop_token &) {
        ++repairs;
        return std::expected<RoadPageGeometry, std::string>((**retained).Page->Geometry);
      }).get());
  CHECK(corrected && repairs == 2 && pages->StoreManifest(*manifest),
        "the next generation repairs the invalid level and restores complete dependencies");
}

void CheckWarm(Tasks &pool,
               const std::shared_ptr<PreparedRoadPages> &pages,
               const Corridors::Job &plan,
               const Corridors::Site &site,
               const std::string &key,
               const RoadPageManifest &expected) {
  using namespace outshine::Test;
  auto warm = RoadPageConstruction::Begin(pool, pages, plan, site, key);
  CHECK(warm && Prepare(**warm, plan, site) && (*warm)->MissRequests() == 0 &&
            (*warm)->Pages() == expected.Pages.size(),
        "warm construction uses only native index hits without source recipes or meshing");
  if (!warm) { return; }
  const auto restored = (*warm)->TakeManifest();
  const auto original = EncodeRoadPageManifest(expected, PreparedRoadPages::ManifestBytesMost);
  const auto encoded = restored
                           ? EncodeRoadPageManifest(*restored, PreparedRoadPages::ManifestBytesMost)
                           : std::nullopt;
  CHECK(original && encoded && *original == *encoded,
        "metadata-only construction retains exactly the same complete manifest words");
}

void CheckBindings(RoadPageManifest &manifest) {
  using namespace outshine::Test;
  const auto encoded = EncodeRoadPageManifest(manifest, PreparedRoadPages::ManifestBytesMost);
  CHECK(encoded.has_value(), "complete native manifest encoding succeeds");
  const auto roundTrip = encoded ? DecodeRoadPageManifest(*encoded, encoded->size()) : std::nullopt;
  CHECK(roundTrip && roundTrip->Pages.size() == manifest.Pages.size() &&
            roundTrip->Pages[0].Key == manifest.Pages[0].Key &&
            roundTrip->Materials.surfaceAt(MaterialInstance(0)) ==
                manifest.Materials.surfaceAt(MaterialInstance(0)),
        "manifest roundtrip retains page ownership and the exact native material");
  if (encoded) {
    CHECK(!DecodeRoadPageManifest(*encoded, encoded->size() - 1),
          "manifest decode obeys the caller byte ceiling");
  }
  const auto original = manifest.Pages[0].Key;
  manifest.Pages[0].Key.assign(64, 'f');
  CHECK(!EncodeRoadPageManifest(manifest, PreparedRoadPages::ManifestBytesMost),
        "page identity binds the shared plan, material, frame, level and owned parts");
  manifest.Pages[0].Key = original;
  const auto bound = manifest.Pages[0].Bounds.Min[0];
  manifest.Pages[0].Bounds.Min[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!EncodeRoadPageManifest(manifest, PreparedRoadPages::ManifestBytesMost),
        "invalid spatial metadata cannot enter the manifest");
  manifest.Pages[0].Bounds.Min[0] = bound;
}

void CheckBuild(const std::filesystem::path &directory,
                Osm::OsmField &vectors,
                const std::vector<Osm::OsmField::Declared> &roads,
                const Corridors::Site &site) {
  using namespace outshine::Test;
  const ProfiledRoadMesher native;
  const Corridors producer(native);
  auto plan = Corridors::Begin(site);
  bool planned = false;
  for (size_t step = 0; step < 4096; ++step) {
    const auto ready = producer.AdvancePlan(*plan, site, 4, 4);
    if (!ready) { break; }
    if (*ready) {
      planned = true;
      break;
    }
  }
  CHECK(planned, "the world consumer starts from complete native plans");
  if (!planned) { return; }
  Geometry expected;
  std::vector<EarthworkStamp> earthworks;
  std::vector<DiagnosticSample> notes;
  CHECK(producer.Lay(site, expected, earthworks, notes), "the original road producer is available");
  auto opened = PreparedRoadPages::Open(directory.string());
  CHECK(opened.has_value(), "the real native product cache opens");
  if (!opened) { return; }
  std::shared_ptr<PreparedRoadPages> pages = std::move(*opened);
  Tasks pool(1);
  const std::string key(64, 'a');
  auto cold = RoadPageConstruction::Begin(pool, pages, *plan, site, key);
  CHECK(cold && Prepare(**cold, *plan, site), "misses generate and persist real native pages");
  CHECK(cold && (*cold)->Pages() >= 2 && (*cold)->MissRequests() == (*cold)->Pages(),
        "more than64 whole intervals create multiple independently read products");
  if (!cold) { return; }
  auto manifest = (*cold)->TakeManifest();
  CHECK(manifest && manifest->Materials.parts() == 0 && manifest->Materials.surfaces() == 1,
        "the manifest owns shared materials and metadata without road meshes");
  CHECK(!(*cold)->TakeManifest(), "a complete manifest transfers ownership once");
  if (!manifest) { return; }
  CHECK(std::ranges::all_of(
            manifest->Pages,
            [](const auto &page) { return !page.Parts.empty() && page.Parts.size() <= 64; }),
        "spatial construction retains the existing bounded whole-part work per page");
  CheckBindings(*manifest);
  const auto manifestKey = pages->StoreManifest(*manifest);
  CHECK(manifestKey.has_value(), "complete pages publish their independent native manifest");
  if (!manifestKey) { return; }
  CheckWarm(pool, pages, *plan, site, key, *manifest);
  GroundRegionAsset root;
  root.Anchor = site.Standing.Anchor();
  root.SurfaceManifest = *manifestKey;
  const auto ground = root.Surfaces.addSurface("ground", Material{});
  CHECK(ground.has_value(), "the base region retains its own material independently of roads");
  if (!ground) { return; }
  root.GroundSurface = ground->index();
  constexpr Vec3 kGroundAlbedo{{0.2, 0.3, 0.4}};
  root.GroundAlbedo = kGroundAlbedo;
  const auto encoded = EncodeGroundRegionAsset(root, size_t{1024} * 1024);
  CHECK(encoded && encoded->size() < 4096 && root.Surfaces.parts() == 0,
        "the regular region package excludes all fine road vertices and indices");
  if (!encoded) { return; }
  const auto metadata = EncodeRoadPageManifest(*manifest, PreparedRoadPages::ManifestBytesMost);
  size_t fineBytes = 0;
  for (const auto &page : manifest->Pages) { fineBytes += static_cast<size_t>(page.ByteCount); }
  std::println(
      "ROAD_MANIFEST region_native_bytes={} manifest_native_bytes={} fine_native_bytes={} pages={}",
      encoded->size(),
      metadata ? metadata->size() : 0,
      fineBytes,
      manifest->Pages.size());
  cold = std::unexpected("released");
  plan.reset();
  pages.reset();
  opened = PreparedRoadPages::Open(directory.string());
  CHECK(opened.has_value(), "a fresh service restores persisted products");
  if (!opened) { return; }
  pages = std::move(*opened);
  auto changed = roads;
  constexpr double kChangedEndpointDeltaDeg = 0.0001;
  changed[0].LatLon.back() += kChangedEndpointDeltaDeg;
  CHECK(vectors.Declare(changed, site.Standing.Anchor()).has_value(),
        "original sources change after their plan is destroyed");
  auto restored = LoadRegion(pool, pages, *encoded);
  CHECK(restored && restored->Loaded && restored->SurfacePages,
        "the real region worker restores material and page metadata without original plan inputs");
  if (!restored || !restored->Loaded || !restored->SurfacePages) { return; }
  auto restore = RoadPageRestoration::Begin(
      pool, pages, restored->SurfacePages, std::nullopt, restored->Loaded->Region.SurfaceManifest);
  Geometry geometry = std::move(restored->Loaded->Region.Surfaces);
  CHECK(restore && Complete(**restore, geometry),
        "the same bounded native reader assembles complete road pages");
  CHECK(restore && (*restore)->Pages() >= 2 && (*restore)->NativeBytes() > 0 &&
            SameMesh(geometry, expected),
        "page boundaries preserve original words, primitive order and material rows");
  auto invalid = RoadPageRestoration::Begin(pool, {}, restored->SurfacePages);
  CHECK(!invalid, "a missing cache service is explicit before a world transition");
  CheckSelection(pool, pages, restored->SurfacePages, *manifestKey, directory, *encoded);
}
}

int main() {
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  const std::vector<std::string> layers{"streets"};
  Osm::OsmField vectors(14, layers);
  std::vector<Osm::OsmField::Declared> roads;
  constexpr size_t kRoadCount = 66;
  constexpr double kEndLatitudeDeg = 49.001;
  for (size_t at = 0; at < kRoadCount; ++at) {
    const double longitude = 8 + static_cast<double>(at) * 0.00016;
    roads.push_back({.Layer = "streets",
                     .Key = "kind",
                     .Value = "residential",
                     .LatLon = {origin.LatitudeDeg, longitude, kEndLatitudeDeg, longitude}});
  }
  CHECK(vectors.Declare(roads, origin).has_value(),
        "fixed independent road intervals are declared");
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "the existing real material rules are loaded");
  Osm::StreetField ways;
  while (!ways.Ingested(vectors)) { (void)ways.Ingest(vectors, vegetation); }
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh triangles;
  const Drape drape{.Surface = triangles,
                    .Field = [](Drape::EastNorth) -> std::optional<double> { return 0; }};
  const std::shared_ptr<const TerrainClassificationMap> classes;
  const Corridors::Site site{.Vectors = &vectors,
                             .Ways = ways,
                             .Materials = materials,
                             .Vegetation = vegetation,
                             .Standing = frame,
                             .Draped = drape,
                             .Classes = classes,
                             .WaterUpM = {},
                             .CensusAt = {},
                             .EyeLatDeg = origin.LatitudeDeg,
                             .EyeLonDeg = origin.LongitudeDeg,
                             .Projection = {.FocalPx = 800}};
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-road-world-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "the private cache directory is created");
  CheckBuild(directory, vectors, roads, site);
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the completed test owns and cleans its native cache");
  return Report();
}
