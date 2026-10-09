#include "Check.h"
#include "Corridors.h"
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
        "roads use production class recipes");
  if (!vegetation.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const std::shared_ptr<const ClassStructure> noClasses;
  const Generators::ProfiledRoadMesher mesher;
  const Generators::Corridors corridors(mesher);
  for (const std::string kind : {"residential", "track"}) {
    Generators::Osm::OsmField vectors(14, layers);
    const std::array declared{Generators::Osm::OsmField::Declared{
        .Layer = "streets", .Key = "kind", .Value = kind, .LatLon = {48.9994, 8, 49.0006, 8}}};
    CHECK(vectors.Declare(declared, origin).has_value(),
          "an ordinary road has no junction or bridge");
    Generators::Osm::StreetField ways;
    CHECK(ways.Ingest(vectors, vegetation) == 1, "the road reaches the generator");
    for (const double slope : {-0.04, 0.0, 0.04}) {
      const Drape drape{.Surface = empty, .Field = [slope](EastNorth at) -> std::optional<double> {
                          return 10.0 + slope * at.NorthM;
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
      CHECK(corridors.Lay(site, geometry, contacts, notes), "the road builds without terrain mesh");
      CHECK(geometry.parts() == 1 && geometry.wellFormed(),
            "a road is independent native geometry");
      if (geometry.parts() != 1) { continue; }
      const auto surface = TriangleBvh::Over(geometry.positionsOf(0), geometry.trianglesOf(0));
      for (int north = -50; north <= 50; ++north) {
        const auto height = surface.Under(0, static_cast<float>(-north));
        CHECK(height && std::abs(*height - (10.0 + slope * north)) < 0.002,
              "the standalone surface continuously follows its planned height profile");
      }
      CHECK(geometry.windingAgainstNormals(0) == 0, "road surface faces agree with their normals");
    }
  }
  return Report();
}
