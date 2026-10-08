#include "Check.h"
#include "Corridors.h"
#include "EarthworkPress.h"
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
        "the road fixture loads production cover rules");
  if (!vegetation.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  Generators::Osm::OsmField vectors(14, layers);
  const std::array declared{Generators::Osm::OsmField::Declared{
      .Layer = "streets", .Key = "kind", .Value = "residential", .LatLon = {48.999, 8, 49.001, 8}}};
  CHECK(vectors.Declare(declared, origin).has_value(), "a road follows the ridge axis");
  Generators::Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, vegetation) == 1, "the road reaches the native generator");
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const std::shared_ptr<const ClassStructure> noClasses;
  const Generators::ProfiledRoadMesher mesher;
  const Generators::Corridors corridors(mesher);
  for (const double slope : {-2., 2.}) {
    const Drape drape{.Surface = empty, .Field = [slope](EastNorth at) -> std::optional<double> {
                        return slope * std::abs(at.EastM);
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
    std::vector<EarthworkStamp> earthworks;
    std::vector<DiagnosticSample> notes;
    CHECK(corridors.Lay(site, geometry, earthworks, notes),
          "the native generator plans contacts on the undeformed centreline");
    CHECK(!earthworks.empty(), "both cut and fill shoulders receive terrain contacts");
    CHECK(std::ranges::all_of(earthworks, [](const auto &stamp) { return stamp.YieldM >= 4; }),
          "planned contact displacement includes the shoulders, not only the centreline");
    const std::array<EastNorth, 2> samples{{{.EastM = -2, .NorthM = 0}, {.EastM = 2, .NorthM = 0}}};
    std::array<double, 2> heights{2 * slope, 2 * slope};
    const auto result = ApplyEarthworkStamps(earthworks, samples, heights, kMostEarthworkM);
    CHECK(result.Structures == 0 && std::abs(heights[0]) < 0.001 && std::abs(heights[1]) < 0.001,
          "both sides of the pavement meet its bed in a valley and on a ridge");
  }
  return Report();
}
