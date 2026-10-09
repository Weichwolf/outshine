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
        "the path uses the production road and terrain recipes");
  if (!vegetation.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  Generators::Osm::OsmField vectors(14, layers);
  const std::array declared{Generators::Osm::OsmField::Declared{
      .Layer = "streets", .Key = "kind", .Value = "path", .LatLon = {49, 7.999, 49, 8.001}}};
  CHECK(vectors.Declare(declared, origin).has_value(), "one ground path is declared");
  Generators::Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, vegetation) == declared.size(), "the path is classified");
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty,
                    .Field = [](EastNorth) -> std::optional<double> { return 10; }};
  const std::shared_ptr<const ClassStructure> noClasses;
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
  const Generators::ProfiledRoadMesher mesher;
  const Generators::Corridors corridors(mesher);
  Geometry geometry;
  std::vector<EarthworkStamp> contacts;
  std::vector<DiagnosticSample> notes;
  CHECK(corridors.Lay(site, geometry, contacts, notes), "the unchanged path is generated");
  CHECK(!contacts.empty(), "a ground surface publishes its physical core even without earthwork");
  for (const auto &contact : contacts) {
    CHECK(contact.ApronM == 0, "an unchanged path needs no surrounding earthwork apron");
  }
  contacts.push_back({.RingEastNorthM = {-100, 4, 100, 4, 100, 6, -100, 6},
                      .LowE = -100,
                      .HighE = 100,
                      .LowN = 4,
                      .HighN = 6,
                      .PlateauM = 8,
                      .CorridorKey = 42,
                      .ApronM = 8,
                      .YieldM = 2,
                      .Fills = true,
                      .Kind = EarthworkKind::Corridor});
  const std::array<EastNorth, 4> samples{{{-20, 0}, {0, 0}, {20, 0}, {0, 3}}};
  for (size_t at = 0; at + 1 < contacts.size(); ++at) {
    CHECK(samples.back().NorthM > contacts[at].HighN,
          "the outside probe lies beyond the path's physical shoulder");
  }
  std::array<double, 4> heights{{10, 10, 10, 10}};
  (void)ApplyEarthworkStamps(contacts, samples, heights, kMostEarthworkM);
  for (size_t at = 0; at < 3; ++at) {
    CHECK(std::abs(heights[at] - 10) < .001,
          "the neighboring cut apron cannot remove terrain under the unchanged path");
  }
  CHECK(heights.back() < 10 && heights.back() > 8,
        "the adjacent earthwork still deforms unoccupied terrain smoothly");
  return Report();
}
