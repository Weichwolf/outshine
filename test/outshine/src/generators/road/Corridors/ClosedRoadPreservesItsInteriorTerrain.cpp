#include "Check.h"
#include "Corridors.h"
#include "EarthworkPress.h"
#include "ProfiledRoadMesher.h"

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "the closed-road fixture uses production classification");
  if (!vegetation.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  Generators::Osm::OsmField vectors(14, layers);
  const std::array declared{Generators::Osm::OsmField::Declared{
      .Layer = "streets",
      .Key = "kind",
      .Value = "residential",
      .LatLon = {48.999, 7.999, 49.001, 7.999, 49.001, 8.001, 48.999, 8.001, 48.999, 7.999}}};
  CHECK(vectors.Declare(declared, origin).has_value(), "a closed road surrounds undeveloped land");
  Generators::Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, vegetation) == 1, "the loop remains a road ribbon");
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const std::shared_ptr<const ClassStructure> noClasses;
  const Generators::ProfiledRoadMesher mesher;
  const Generators::Corridors corridors(mesher);
  for (const double amplitude : {-10., 10.}) {
    const Drape drape{
        .Surface = empty, .Field = [amplitude](EastNorth at) -> std::optional<double> {
          return 0.2 * at.EastM +
                 amplitude * std::exp(-(at.EastM * at.EastM + at.NorthM * at.NorthM) / 400.);
        }};
    const Generators::Corridors::Site site{.Vectors = &vectors,
                                           .Ways = ways,
                                           .Materials = materials,
                                           .Vegetation = vegetation,
                                           .Standing = frame,
                                           .Draped = drape,
                                           .Classes = noClasses,
                                           .EyeLatDeg = origin.LatitudeDeg,
                                           .EyeLonDeg = origin.LongitudeDeg,
                                           .Projection = {.FocalPx = 800}};
    Geometry geometry;
    std::vector<EarthworkStamp> contacts;
    std::vector<DiagnosticSample> notes;
    CHECK(corridors.Lay(site, geometry, contacts, notes) && !contacts.empty(),
          "the road still generates its pavement contacts");
    const std::array<EastNorth, 1> centre{{{0, 0}}};
    std::array<double, 1> heights{amplitude};
    const auto pressed = ApplyEarthworkStamps(contacts, centre, heights, kMostEarthworkM);
    CHECK(pressed.Structures == 0 && heights[0] == amplitude,
          "closing a road neither cuts a hill nor fills a valley inside its loop");
  }
  return Report();
}
