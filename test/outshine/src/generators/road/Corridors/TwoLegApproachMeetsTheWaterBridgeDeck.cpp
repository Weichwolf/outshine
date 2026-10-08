#include "src/generators/road/Corridors.h"
#include "src/generators/road/ProfiledRoadMesher.h"
#include "Check.h"

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

void CheckBridgeJoint(std::span<const outshine::EarthworkStamp> earthworks,
                      outshine::EastNorth at) {
  using namespace outshine;
  using namespace outshine::Test;
  std::optional<double> deck;
  std::optional<double> ramp;
  for (const auto &stamp : earthworks) {
    if (stamp.LowN > at.NorthM + 0.001 || stamp.HighN < at.NorthM - 0.001) { continue; }
    if (stamp.Kind == EarthworkKind::Clearance) { deck = stamp.WantsAt(at); }
    if (stamp.Kind == EarthworkKind::Corridor) { ramp = stamp.WantsAt(at); }
  }
  CHECK(deck && *deck > 1, "the bridge stands above the water without another street crossing");
  CHECK(deck && ramp && std::fabs(*deck - *ramp) < 0.01,
        "a two-arm approach reaches the bridge deck without requiring a three-arm junction");
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "the fixture uses production road and water rules");
  if (!vegetation.Ready()) { return Report(); }
  int water = -1;
  for (size_t row = 0; row < vegetation.TemplateCount(); ++row) {
    if (vegetation.Rows()[row].GroundClass == materials.Find("water")) {
      water = static_cast<int>(row);
      break;
    }
  }
  CHECK(water >= 0, "a water classification is available");
  if (water < 0) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  for (const int variant : {0, 1, 2, 3}) {
    const bool betweenBridges = variant >= 2;
    Generators::Osm::OsmField vectors(14, layers);
    std::vector<Generators::Osm::OsmField::Declared> declared{{.Layer = "streets",
                                                               .Key = "kind",
                                                               .Value = "residential",
                                                               .Bridge = true,
                                                               .Level = 1,
                                                               .LatLon = {49.0, 8, 49.001, 8}},
                                                              {.Layer = "streets",
                                                               .Key = "kind",
                                                               .Value = "residential",
                                                               .LatLon = {48.999, 8, 49.0, 8}}};
    if (betweenBridges) {
      declared[0].LatLon = {48.999, 8, 49.0, 8};
      declared[1].LatLon = {49.0, 8, 49.0002, 8};
      declared.push_back({.Layer = "streets",
                          .Key = "kind",
                          .Value = "residential",
                          .Bridge = true,
                          .Level = 1,
                          .LatLon = {49.0002, 8, 49.0012, 8}});
    }
    if (variant % 2 != 0) {
      std::swap(declared[1].LatLon[0], declared[1].LatLon[2]);
      std::swap(declared[1].LatLon[1], declared[1].LatLon[3]);
    }
    CHECK(vectors.Declare(declared, origin).has_value(),
          "a bridge and its land approach are declared");
    Generators::Osm::StreetField ways;
    CHECK(ways.Ingest(vectors, vegetation) == declared.size(),
          "both adjoining roads reach the generator");
    const TangentFrame standing = TangentFrame::At(origin);
    auto grid = std::make_shared<ClassStructure::Grid>();
    grid->W = grid->H = 1;
    grid->OrgE = -500;
    grid->OrgN = betweenBridges ? -500 : 0;
    grid->CellM = 1000;
    grid->Cells = {static_cast<uint32_t>(water) | (1u << 8), 0};
    const std::shared_ptr<const ClassStructure> classes = std::make_shared<ClassStructure>(
        standing, grid, std::make_shared<const ClassStructure::Grid>(), ClassStructure::FromRun{});
    const TriangleBvh empty = TriangleBvh::Over({}, {});
    const Drape drape{.Surface = empty,
                      .Field = [betweenBridges](EastNorth at) -> std::optional<double> {
                        return betweenBridges && at.NorthM > 30 ? 2 : 0;
                      }};
    const Generators::Corridors::Site site{
        .Vectors = &vectors,
        .Ways = ways,
        .Materials = materials,
        .Vegetation = vegetation,
        .Standing = standing,
        .Draped = drape,
        .Classes = classes,
        .WaterUpM = [](LongitudeLatitude) { return std::optional<double>{0}; },
        .EyeLatDeg = origin.LatitudeDeg,
        .EyeLonDeg = origin.LongitudeDeg,
        .Projection = {.FocalPx = 800}};
    const Generators::ProfiledRoadMesher mesher;
    const Generators::Corridors corridors(mesher);
    Geometry geometry;
    std::vector<EarthworkStamp> earthworks;
    std::vector<DiagnosticSample> notes;
    CHECK(corridors.Lay(site, geometry, earthworks, notes) && geometry.wellFormed(),
          "the water crossing generates native road products");
    CheckBridgeJoint(earthworks, {0, 0});
    if (betweenBridges) {
      CheckBridgeJoint(earthworks,
                       standing.ToLocalGroundPosition({.LongitudeDeg = 8, .LatitudeDeg = 49.0002}));
    }
  }
  return Report();
}
