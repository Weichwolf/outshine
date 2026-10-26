#include "Check.h"
#include "Corridors.h"
#include "ProfiledRoadMesher.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {
struct ObservedMesher final : public outshine::RoadMesher {
public:
  mutable std::vector<float> Positions;

  outshine::RoadMeshingStats Sweep(std::span<const outshine::RoadStation> along,
                                   outshine::RoadSweep how,
                                   outshine::RoadMeshBuffers &into) const override {
    const size_t first = into.PositionM.size();
    const auto result = Native_.Sweep(along, how, into);
    const auto appended = std::span(into.PositionM).subspan(first);
    Positions.insert(Positions.end(), appended.begin(), appended.end());
    return result;
  }

  void Junction(std::span<const outshine::RoadGate> gates,
                outshine::RoadPlane plane,
                const outshine::Vec3f &colour,
                outshine::RoadMeshBuffers &into) const override {
    Native_.Junction(gates, plane, colour, into);
  }

  outshine::Generators::ProfiledRoadMesher Native_;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  outshine::Generators::GroundMaterialCatalog materials;
  outshine::Generators::RegionSurfaceRecipes vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "the contact uses the production road recipes");
  if (!vegetation.Ready()) { return Report(); }
  for (const auto *kind : {"path", "residential", "primary"}) {
    const std::array<std::string, 1> layers{"streets"};
    Generators::Osm::OsmField vectors(14, layers);
    const std::array declared{Generators::Osm::OsmField::Declared{
        .Layer = "streets", .Key = "kind", .Value = kind, .LatLon = {48.999, 8, 49.001, 8}}};
    CHECK(vectors.Declare(declared, origin).has_value(), "a straight physical road is declared");
    Generators::Osm::StreetField ways;
    CHECK(ways.Ingest(vectors, vegetation) == 1, "the road is classified");
    const TangentFrame frame = TangentFrame::At(origin);
    const TriangleBvh empty = TriangleBvh::Over({}, {});
    const Drape drape{.Surface = empty,
                      .Field = [](EastNorth) -> std::optional<double> { return 10; }};
    const std::shared_ptr<const TerrainClassificationMap> noClasses;
    const Generators::Corridors::Site site{.Vectors = &vectors,
                                           .Ways = ways,
                                           .Materials = materials,
                                           .Vegetation = vegetation,
                                           .Standing = frame,
                                           .Draped = drape,
                                           .Classes = noClasses,
                                           .WaterUpM = {},
                                           .CensusAt = {},
                                           .EyeLatDeg = origin.LatitudeDeg,
                                           .EyeLonDeg = origin.LongitudeDeg,
                                           .Projection = {.FocalPx = 800}};
    ObservedMesher mesher;
    const Generators::Corridors corridors(mesher);
    Geometry geometry;
    std::vector<EarthworkStamp> contacts;
    std::vector<DiagnosticSample> notes;
    CHECK(corridors.Lay(site, geometry, contacts, notes), "the physical road is generated");
    CHECK(!contacts.empty() && !mesher.Positions.empty(), "road contacts and real vertices exist");
    double renderedHalfWidth = 0;
    for (size_t at = 0; at < mesher.Positions.size(); at += 3) {
      renderedHalfWidth =
          std::max(renderedHalfWidth, std::abs(static_cast<double>(mesher.Positions[at])));
    }
    double contactHalfWidth = 0;
    for (const auto &stamp : contacts) {
      for (size_t at = 0; at < stamp.RingEastNorthM.size(); at += 2) {
        contactHalfWidth = std::max(contactHalfWidth, std::abs(stamp.RingEastNorthM[at]));
      }
    }
    CHECK_NEAR(contactHalfWidth,
               renderedHalfWidth,
               0.0001,
               "metres",
               "hard terrain follows the rendered shoulders rather than an invented wide shelf");
  }
  return Report();
}
