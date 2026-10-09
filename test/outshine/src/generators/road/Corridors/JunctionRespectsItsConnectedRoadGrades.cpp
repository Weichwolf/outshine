#include "Check.h"
#include "Corridors.h"
#include "ProfiledRoadMesher.h"

#include <algorithm>
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
        "junctions use production road grade limits");
  if (!vegetation.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty,
                    .Field = [](EastNorth at) -> std::optional<double> { return .3 * at.NorthM; }};
  const std::shared_ptr<const ClassStructure> noClasses;
  const Generators::ProfiledRoadMesher mesher;
  const Generators::Corridors corridors(mesher);
  for (const int variant : {0, 1, 2, 3}) {
    Generators::Osm::OsmField vectors(14, layers);
    const std::string kind = variant == 1 ? "path" : variant == 3 ? "rail" : "residential";
    const std::array declared{
        Generators::Osm::OsmField::Declared{
            .Layer = "streets", .Key = "kind", .Value = kind, .LatLon = {49, 8, 49.001, 8}},
        Generators::Osm::OsmField::Declared{
            .Layer = "streets", .Key = "kind", .Value = kind, .LatLon = {49, 8, 48.999, 8}},
        Generators::Osm::OsmField::Declared{.Layer = "streets",
                                            .Key = "kind",
                                            .Value = variant == 2 ? "path" : kind,
                                            .LatLon = {49, 8, 49, 8.001}}};
    CHECK(vectors.Declare(declared, origin).has_value(), "three roads meet at one real node");
    Generators::Osm::StreetField ways;
    CHECK(ways.Ingest(vectors, vegetation) == declared.size(), "all junction arms are classified");
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
          "the junction publishes its native terrain contact before its arms");
    if (contacts.empty()) { continue; }
    double most = 1;
    for (const auto &way : ways.Ways()) {
      most = std::min(most, way.MaxGradient > 0 ? double{way.MaxGradient} : .10);
    }
    if (variant == 3) {
      CHECK(ways.Ways()[0].MaxGradient == 0, "the supplied rail recipe has no grade bound");
    }
    const auto &junction = contacts.front();
    CHECK(std::abs(junction.AtE) < .001 && std::abs(junction.AtN) < .001 &&
              std::abs(junction.SlopeE) < .0001 && std::abs(junction.SlopeN - most) < .0001,
          "the junction plane preserves uphill direction and respects every connected road class");
  }
  return Report();
}
