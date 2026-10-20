#include "RoadPageConstruction.h"
#include "ProfiledRoadMesher.h"
#include "Check.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
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

bool Complete(RoadPageConstruction &build,
              const Corridors::Job &plan,
              const Corridors::Site &site,
              Geometry &into) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  while (std::chrono::steady_clock::now() < until) {
    const auto ready = build.Advance(plan, site, into);
    if (!ready) { return false; }
    if (*ready) { return true; }
    std::this_thread::yield();
  }
  return false;
}

bool SameMesh(const Geometry &parts, const Geometry &whole) {
  if (whole.parts() != 1 || !parts.wellFormed()) { return false; }
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
        !std::ranges::equal(positions,
                            whole.positionsOf(0).subspan(3 * vertices, positions.size())) ||
        !std::ranges::equal(normals, whole.normalsOf(0).subspan(3 * vertices, normals.size())) ||
        !std::ranges::equal(colours, whole.coloursOf(0).subspan(4 * vertices, colours.size()))) {
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
  Geometry built;
  CHECK(cold && Complete(**cold, *plan, site, built), "misses generate and load real world pages");
  CHECK(cold && (*cold)->Pages() >= 2 && (*cold)->MissRequests() == (*cold)->Pages(),
        "more than64 whole intervals create multiple independently read products");
  CHECK(SameMesh(built, expected), "all page geometry and materials preserve the original mesh");
  cold = std::unexpected("released");
  pages.reset();
  opened = PreparedRoadPages::Open(directory.string());
  CHECK(opened.has_value(), "a fresh service restores only persisted road products");
  if (!opened) { return; }
  pages = std::move(*opened);
  auto warm = RoadPageConstruction::Begin(pool, pages, *plan, site, key);
  auto changed = roads;
  changed[0].LatLon.back() += 0.0001;
  CHECK(vectors.Declare(changed, site.Standing.Anchor()).has_value(),
        "original recipe inputs become stale");
  CHECK(!Corridors::PageRecipe(*plan, site, std::span<const RoadPagePart>{}),
        "source preparation would reject an attempted original recipe reconstruction");
  Geometry restored;
  CHECK(warm && Complete(**warm, *plan, site, restored),
        "cache hits never enter recipe preparation or meshing despite stale original sources");
  CHECK(warm && (*warm)->MissRequests() == 0 && (*warm)->Pages() >= 2 &&
            (*warm)->NativeBytes() > 0 && SameMesh(restored, expected),
        "fresh-service hits restore identical complete pages with ready shared materials");
  auto invalid = RoadPageConstruction::Begin(pool, {}, *plan, site, key);
  CHECK(!invalid, "a missing cache service is explicit before starting a world transition");
}
}

int main() {
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  const std::vector<std::string> layers{"streets"};
  Osm::OsmField vectors(14, layers);
  std::vector<Osm::OsmField::Declared> roads;
  for (size_t at = 0; at < 66; ++at) {
    const double longitude = 8 + static_cast<double>(at) * 0.00016;
    roads.push_back({.Layer = "streets",
                     .Key = "kind",
                     .Value = "residential",
                     .LatLon = {49, longitude, 49.001, longitude}});
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
