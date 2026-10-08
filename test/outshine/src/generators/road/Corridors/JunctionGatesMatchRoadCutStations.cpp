#include "Check.h"
#include "Corridors.h"
#include "ProfiledRoadMesher.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
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
        "junctions use production road rules");
  if (!vegetation.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty,
                    .Field = [](EastNorth at) -> std::optional<double> { return .1 * at.NorthM; }};
  const std::shared_ptr<const ClassStructure> noClasses;
  const Generators::ProfiledRoadMesher mesher;
  const Generators::Corridors corridors(mesher);
  for (const bool reverse : {false, true}) {
    Generators::Osm::OsmField vectors(14, layers);
    std::array declared{
        Generators::Osm::OsmField::Declared{.Layer = "streets",
                                            .Key = "kind",
                                            .Value = "residential",
                                            .LatLon = {49, 8, 49.00002, 8, 49.001, 8}},
        Generators::Osm::OsmField::Declared{.Layer = "streets",
                                            .Key = "kind",
                                            .Value = "residential",
                                            .LatLon = {49, 8, 48.99998, 8, 48.999, 8}},
        Generators::Osm::OsmField::Declared{.Layer = "streets",
                                            .Key = "kind",
                                            .Value = "residential",
                                            .LatLon = {49, 8, 49, 8.00002, 49, 8.001}}};
    if (reverse) {
      for (auto &road : declared) {
        std::swap(road.LatLon[0], road.LatLon[4]);
        std::swap(road.LatLon[1], road.LatLon[5]);
      }
    }
    CHECK(vectors.Declare(declared, origin).has_value(),
          "source stations precede the junction cut");
    Generators::Osm::StreetField ways;
    CHECK(ways.Ingest(vectors, vegetation) == declared.size(),
          "all junction arms reach the generator");
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
    CHECK(corridors.Lay(site, geometry, contacts, notes) && contacts.size() >= 3,
          "the junction and adjoining terrain contacts are built");
    CHECK(!contacts.empty() && std::abs(contacts.front().AtE) < .00001 &&
              std::abs(contacts.front().AtN) < .00001,
          "a three-arm junction publishes a contact at the shared source node");
    double mostDriftM = 0;
    const EarthworkStamp *witness = nullptr;
    // The junction footing is below its pavement; adjoining contacts stay on the surface.
    for (size_t at = 1; at < contacts.size(); ++at) {
      const auto &contact = contacts[at];
      const double driftM = std::abs(contact.PlateauM - .1 * contact.AtN);
      if (driftM > mostDriftM) {
        mostDriftM = driftM;
        witness = &contact;
      }
    }
    if (witness != nullptr && mostDriftM >= .00001) {
      std::fprintf(stderr,
                   "contact drift %.9f m at EN %.9f %.9f, height %.9f m\n",
                   mostDriftM,
                   witness->AtE,
                   witness->AtN,
                   witness->PlateauM);
    }
    CHECK(mostDriftM < .00001,
          "adjoining road cut stations follow the compliant "
          "analytic slope in either direction");
  }
  return Report();
}
