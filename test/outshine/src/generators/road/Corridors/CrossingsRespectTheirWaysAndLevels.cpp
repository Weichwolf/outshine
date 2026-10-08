#include "Check.h"
#include "Corridors.h"
#include "ProfiledRoadMesher.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

void CheckCrossings(int variant) {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "crossings use production road classification");
  if (!vegetation.Ready()) { return; }
  const std::array<std::string, 1> layers{"streets"};
  Generators::Osm::OsmField vectors(14, layers);
  const std::array declared{
      Generators::Osm::OsmField::Declared{
          .Layer = "streets",
          .Key = "kind",
          .Value = "residential",
          .Bridge = variant == 1 || variant == 3,
          .Level = variant >= 2 ? 1 : 0,
          .LatLon = variant >= 3 ? std::vector<double>{49, 7.998, 49, 8, 49, 8.002}
                                 : std::vector<double>{49, 7.998, 49, 8.002}},
      Generators::Osm::OsmField::Declared{
          .Layer = "streets",
          .Key = "kind",
          .Value = "residential",
          .LatLon = variant >= 3 ? std::vector<double>{48.999, 8, 49, 8, 49.001, 8}
                                 : std::vector<double>{48.999, 8, 49.001, 8}},
      Generators::Osm::OsmField::Declared{.Layer = "streets",
                                          .Key = "kind",
                                          .Value = "residential",
                                          .LatLon = {48.999, 8.00007, 49.001, 8.00007}}};
  CHECK(vectors.Declare(declared, origin).has_value(), "two nearby crossings are declared");
  Generators::Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, vegetation) == declared.size(), "all three roads are classified");
  auto network = Path::Network::Create({.CellM = 32}, {});
  CHECK(network.has_value(), "the native network is created");
  if (!network) { return; }
  for (size_t lane = 0; lane < ways.Ways().size(); ++lane) {
    const auto &way = ways.Ways()[lane];
    CHECK(
        network
            ->Lay(vectors.Points().subspan(2u * way.FirstPoint, 2u * way.PointCount), {.Tag = lane})
            .has_value(),
        "the network retains the source-way association");
  }
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty,
                    .Field = [](EastNorth) -> std::optional<double> { return 0; }};
  const std::shared_ptr<const ClassStructure> noClasses;
  const Generators::ProfiledRoadMesher mesher;
  const Generators::Corridors corridors(mesher);
  const Generators::Corridors::Site site{.Vectors = &vectors,
                                         .Ways = ways,
                                         .Materials = materials,
                                         .Vegetation = vegetation,
                                         .Network = &*network,
                                         .Standing = frame,
                                         .Draped = drape,
                                         .Classes = noClasses,
                                         .EyeLatDeg = origin.LatitudeDeg,
                                         .EyeLonDeg = origin.LongitudeDeg,
                                         .Projection = {.FocalPx = 800}};
  Geometry geometry;
  std::vector<EarthworkStamp> contacts;
  std::vector<DiagnosticSample> notes;
  CHECK(corridors.Lay(site, geometry, contacts, notes), "the crossing products are generated");
  const auto junctions =
      std::ranges::find(notes, "streets: junctions shaped", &DiagnosticSample::Name);
  CHECK(junctions != notes.end(), "the native topology reports its junction count");
  if (junctions != notes.end()) {
    CHECK(
        junctions->Value == (variant == 0 ? 2 : 0),
        "nearby intersections remain separate; bridges and distinct levels never become junctions");
  }
}

}

int main() {
  for (const int variant : {0, 1, 2, 3, 4}) { CheckCrossings(variant); }
  return outshine::Test::Report();
}
