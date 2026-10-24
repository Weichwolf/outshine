#include "Check.h"
#include "Corridors.h"
#include "ProfiledRoadMesher.h"
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr LongitudeLatitude kOrigin{.LongitudeDeg = 8, .LatitudeDeg = 49};
constexpr double kGapM = .282;
constexpr double kDistantGapM = .8;
constexpr double kExtentM = 20;
constexpr size_t kPlanningLimit = 10000;

struct ConnectionCase {
  double GapM = 0;
  bool Connects = false;
  bool SharedSourceKnot = false;
};

std::vector<double> Coordinates(const TangentFrame &frame, EastNorth a, EastNorth b) {
  const auto first = frame.ApproximateGeographicAt(a);
  const auto last = frame.ApproximateGeographicAt(b);
  return {first.LatitudeDeg, first.LongitudeDeg, last.LatitudeDeg, last.LongitudeDeg};
}

void CheckConnection(const Ground::GroundMaterials &materials,
                     const Ground::VegetationTemplates &vegetation,
                     ConnectionCase test) {
  const std::array<std::string, 1> layers{"streets"};
  const auto frame = TangentFrame::At(kOrigin);
  auto target =
      Coordinates(frame, {.EastM = -kExtentM, .NorthM = 0}, {.EastM = kExtentM, .NorthM = 0});
  if (test.SharedSourceKnot) {
    target.insert(target.begin() + 2, {kOrigin.LatitudeDeg, kOrigin.LongitudeDeg});
  }
  Osm::OsmField vectors(14, layers);
  const std::array declarations{
      Osm::OsmField::Declared{.Layer = "streets",
                              .Key = "kind",
                              .Value = "steps",
                              .LatLon = Coordinates(frame,
                                                    {.EastM = 0, .NorthM = -kExtentM},
                                                    {.EastM = 0, .NorthM = -test.GapM})},
      Osm::OsmField::Declared{
          .Layer = "streets", .Key = "kind", .Value = "footway", .LatLon = target}};
  CHECK(vectors.Declare(declarations, kOrigin).has_value(),
        "the live fixture owns source metadata");
  Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, vegetation) == 2,
        "steps retain their source kind after path admission");
  const auto empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty,
                    .Field = [](EastNorth) -> std::optional<double> { return 0; }};
  const std::shared_ptr<const TerrainClassificationMap> noClasses;
  const Corridors::Site site{.Vectors = &vectors,
                             .Ways = ways,
                             .Materials = materials,
                             .Vegetation = vegetation,
                             .Standing = frame,
                             .Draped = drape,
                             .Classes = noClasses,
                             .EyeLatDeg = kOrigin.LatitudeDeg,
                             .EyeLonDeg = kOrigin.LongitudeDeg,
                             .Projection = {.FocalPx = 800}};
  const ProfiledRoadMesher mesher;
  const Corridors corridors(mesher);
  auto job = Corridors::Begin(site);
  CHECK(job != nullptr, "the actual cached road-page pipeline begins");
  if (!job) { return; }
  bool ready = false;
  for (size_t step = 0; step < kPlanningLimit && !ready; ++step) {
    const auto next = corridors.AdvancePlan(*job, site, 1, 1);
    CHECK(next.has_value(), "every bounded native world-plan stage completes");
    if (!next) { return; }
    ready = *next;
  }
  CHECK(ready, "the complete world plan terminates without an unbounded retry");
  bool counted = false;
  for (const auto &measure : Corridors::PlanMeasures(*job)) {
    if (measure.Name == "footpaths: joined ends") {
      counted = true;
      CHECK(measure.Value == (test.Connects && !test.SharedSourceKnot ? 1 : 0),
            "native connection telemetry distinguishes a constructed join from an existing one");
    }
  }
  CHECK(counted, "the world consumer can publish the actual native plan counters");
  const auto parts = Corridors::PlannedParts(*job);
  CHECK(parts.has_value(), "the native source plan produces real cacheable road-page parts");
  if (!parts) { return; }
  const auto recipe = Corridors::PageRecipe(*job, site, *parts);
  CHECK(recipe.has_value(), "the actual world-page consumer receives the corrected axes");
  if (!recipe) { return; }
  CHECK(recipe->Junctions.size() == (test.Connects ? 1u : 0u),
        "only a qualified nearby gap creates a shared junction");
  CHECK(recipe->Intervals.size() == (test.Connects ? 3u : 2u),
        "the target is split at the new node; no tiny leftover branch edge is published");
  if (test.Connects && !recipe->Junctions.empty()) {
    CHECK(recipe->Junctions.front().Node != 0 && recipe->Junctions.front().Gates.size() == 3,
          "one actual three-arm junction owns both source paths");
  }
  const auto mesh = MeshRoadPage(mesher, *recipe);
  CHECK(mesh && !mesh->Mesh.Index.empty(), "the generated product is consumed by the real mesher");
  CHECK(mesh && mesh->Swept.Refused == 0 && mesh->Diagnostics.empty(),
        "every requested interval and junction is meshed without a partial or empty product");
}
}

int main() {
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "the integration uses production material and path rules");
  if (!vegetation.Ready()) { return Report(); }
  CheckConnection(materials, vegetation, {.GapM = kGapM, .Connects = true});
  CheckConnection(materials, vegetation, {.GapM = kDistantGapM});
  CheckConnection(materials, vegetation, {.Connects = true, .SharedSourceKnot = true});
  return Report();
}
