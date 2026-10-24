#include "Corridors.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

class NoMesh final : public RoadMesher {
public:
  mutable size_t Calls = 0;

  [[nodiscard]] RoadMeshingStats Sweep(std::span<const RoadStation> along,
                                       RoadSweep surface,
                                       RoadMeshBuffers &mesh) const override {
    ++Calls;
    return {.Refused = 1, .Rejections = {}};
  }

  void Junction(std::span<const RoadGate> gates,
                RoadPlane plane,
                const Vec3f &colour,
                RoadMeshBuffers &mesh) const override {
    ++Calls;
  }
};

struct PlanResult {
  std::vector<RoadPagePart> Parts;
  std::vector<EarthworkStamp> Contacts;
  size_t Advances = 0;
  size_t HeightQueries = 0;
};

std::optional<PlanResult>
Plan(const Corridors &corridors, const Corridors::Site &site, const size_t &heightQueries) {
  constexpr size_t kAdvancesMost = 2048;
  const auto queriesBefore = heightQueries;
  auto job = Corridors::Begin(site);
  for (size_t step = 1; step <= kAdvancesMost; ++step) {
    const auto ready = corridors.AdvancePlan(*job, site, 1, 1);
    if (!ready) { return std::nullopt; }
    if (!*ready) { continue; }
    auto parts = Corridors::PlannedParts(*job);
    auto contacts = Corridors::TakeTerrainContacts(*job);
    if (!parts || !contacts) { return std::nullopt; }
    return PlanResult{.Parts = std::move(*parts),
                      .Contacts = std::move(*contacts),
                      .Advances = step,
                      .HeightQueries = heightQueries - queriesBefore};
  }
  return std::nullopt;
}
}

int main(int argc, char **argv) {
  using namespace outshine::Test;
  const bool withoutProjection = argc == 2 && std::string_view(argv[1]) == "--without-projection";
  CHECK(argc == 1 || withoutProjection, "the diagnostic accepts only its explicit projection mode");
  constexpr LongitudeLatitude kOrigin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  const std::array<std::string, 1> layers{"streets"};
  Osm::OsmField vectors(14, layers);
  const std::array<Osm::OsmField::Declared, 3> roads{
      {{.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .LatLon = {48.9998, 7.9990, 49.0, 8.0, 49.0002, 8.0010}},
       {.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .Bridge = true,
        .Level = 1,
        .LatLon = {48.9990, 8.0, 49.0, 8.0, 49.0010, 8.0}},
       {.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .LatLon = {49.0, 8.0, 49.0007, 8.0008}}}};
  CHECK(vectors.Declare(roads, kOrigin), "the fixed source scope contains three declared roads");
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "the bounded source scope uses the production transport recipes");
  if (!vegetation.Ready()) { return Report(); }
  Osm::StreetField ways;
  while (!ways.Ingested(vectors)) { (void)ways.Ingest(vectors, vegetation); }
  const auto frame = TangentFrame::At(kOrigin);
  const auto surface = TriangleBvh::Over({}, {});
  size_t heightQueries = 0;
  const Drape drape{.Surface = surface, .Field = [&](Drape::EastNorth) -> std::optional<double> {
                      ++heightQueries;
                      return 0;
                    }};
  const std::shared_ptr<const TerrainClassificationMap> classes;
  Corridors::Site nearSite{.Vectors = &vectors,
                           .Ways = ways,
                           .Materials = materials,
                           .Vegetation = vegetation,
                           .Standing = frame,
                           .Draped = drape,
                           .Classes = classes,
                           .WaterUpM = {},
                           .CensusAt = {},
                           .EyeLatDeg = kOrigin.LatitudeDeg,
                           .EyeLonDeg = kOrigin.LongitudeDeg,
                           .Projection = {.FocalPx = 800}};
  if (withoutProjection) { nearSite.Projection = {}; }
  auto farSite = nearSite;
  farSite.EyeLatDeg += 1;
  const NoMesh mesher;
  const Corridors corridors(mesher);
  const auto near = Plan(corridors, nearSite, heightQueries);
  const auto far = Plan(corridors, farSite, heightQueries);
  CHECK(near && far, "both camera states complete bounded native planning");
  if (!near || !far) { return Report(); }
  std::cout << "TRANSPORT_PLAN_SCOPE projection=" << (withoutProjection ? "disabled" : "enabled")
            << " ways=" << ways.Ways().size() << " near_parts=" << near->Parts.size()
            << " far_parts=" << far->Parts.size() << " near_contacts=" << near->Contacts.size()
            << " far_contacts=" << far->Contacts.size() << " near_advances=" << near->Advances
            << " far_advances=" << far->Advances << " near_height_queries=" << near->HeightQueries
            << " far_height_queries=" << far->HeightQueries << " mesher_calls=" << mesher.Calls
            << '\n';
  CHECK(!near->Parts.empty() && !near->Contacts.empty(),
        "the fixture has functional roads and terrain contact profiles");
  CHECK(near->Parts == far->Parts,
        "camera position cannot remove parts from the same functional transport scope");
  CHECK(near->Contacts == far->Contacts,
        "camera position cannot alter functional profiles and terrain contacts");
  CHECK(mesher.Calls == 0, "functional planning performs no fine road meshing");
  return Report();
}
