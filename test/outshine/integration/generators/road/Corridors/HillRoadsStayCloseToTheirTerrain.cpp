#include "Check.h"
#include "Corridors.h"
#include "EarthworkDeformation.h"
#include "ProfiledRoadMesher.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
constexpr double kGroundHeightM = 400;
constexpr double kHillGradient = .16;
constexpr double kContactToleranceM = .75;
constexpr double kGradientTolerance = 1e-6;

double TerrainAt(outshine::EastNorth at) {
  return kGroundHeightM + kHillGradient * at.NorthM;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  constexpr double latitudeStepDeg = .0036;
  outshine::Generators::GroundMaterialCatalog materials;
  outshine::Generators::RegionSurfaceRecipes recipes;
  CHECK(materials.Load("data/world/ground-materials.json") &&
            recipes.Load("data/world/vegetation.json", materials),
        "the hillside uses the actual built-in road recipes");
  if (!recipes.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty,
                    .Field = [](EastNorth at) -> std::optional<double> { return TerrainAt(at); }};
  const std::shared_ptr<const TerrainClassificationMap> noClasses;
  const Generators::ProfiledRoadMesher mesher;
  const Generators::Corridors corridors(mesher);
  for (const bool reversed : {false, true}) {
    Generators::Osm::OsmField vectors(14, layers);
    std::vector<double> axis;
    for (int i = 0; i < 3; ++i) {
      axis.push_back(origin.LatitudeDeg + (reversed ? 2 - i : i) * latitudeStepDeg);
      axis.push_back(origin.LongitudeDeg);
    }
    const std::array declared{Generators::Osm::OsmField::Declared{
        .Layer = "streets", .Key = "kind", .Value = "residential", .LatLon = axis}};
    CHECK(vectors.Declare(declared, origin).has_value(), "an ordinary hillside road is declared");
    Generators::Osm::StreetField ways;
    CHECK(ways.Ingest(vectors, recipes) == 1,
          "the normal source classification selects its recipe");
    const Generators::Corridors::Site site{.Vectors = &vectors,
                                           .Ways = ways,
                                           .Materials = materials,
                                           .Vegetation = recipes,
                                           .Standing = frame,
                                           .Draped = drape,
                                           .Classes = noClasses,
                                           .WaterUpM = {},
                                           .CensusAt = {},
                                           .EyeLatDeg = origin.LatitudeDeg,
                                           .EyeLonDeg = origin.LongitudeDeg,
                                           .Projection = {.FocalPx = 800}};
    Geometry geometry;
    std::vector<EarthworkStamp> contacts;
    std::vector<DiagnosticSample> notes;
    CHECK(corridors.Lay(site, geometry, contacts, notes) && !contacts.empty(),
          "the whole hill road builds with native terrain contacts in either source direction");
    std::vector<EastNorth> points;
    std::vector<double> heights;
    for (const auto &contact : contacts) {
      CHECK(std::hypot(contact.SlopeE, contact.SlopeN) <=
                ways.Ways().front().MaxGradient + kGradientTolerance,
            "every planned contact respects the classified road gradient");
      const EastNorth at{.EastM = contact.AtE, .NorthM = contact.AtN};
      CHECK(std::abs(contact.WantsAt(at) - TerrainAt(at)) < kContactToleranceM,
            "a plausible hillside road follows its ground without a deep artificial cut");
      points.push_back(at);
      heights.push_back(TerrainAt(at));
    }
    const auto pressed = ApplyEarthworkStamps(contacts, points, heights, kMostEarthworkM);
    CHECK(std::ranges::all_of(pressed.Refused, [](auto refused) { return refused == 0; }),
          "all hill-road contacts fit the existing terrain allowance");
    for (size_t i = 0; i < points.size(); ++i) {
      CHECK(std::abs(heights[i] - TerrainAt(points[i])) < kContactToleranceM,
            "the final terrain remains close to the original continuous hillside");
    }
  }
  return Report();
}
