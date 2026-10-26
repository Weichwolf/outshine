#include "Check.h"
#include "Corridors.h"
#include "EarthworkDeformation.h"
#include "ProfiledRoadMesher.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {
void CheckBoundaryContinuity(std::span<const outshine::EarthworkStamp> contacts) {
  using namespace outshine;
  const auto &path = contacts.front();
  const auto &ring = path.RingEastNorthM;
  const double runE = ring[6] - ring[4];
  const double runN = ring[7] - ring[5];
  const double lengthM = std::hypot(runE, runN);
  constexpr double stepM = 0.001;
  constexpr double contactToleranceM = 1.e-6;
  constexpr double maximumSlope = 0.01;
  const EastNorth edge{.EastM = .5 * (ring[4] + ring[6]), .NorthM = .5 * (ring[5] + ring[7])};
  const std::array<EastNorth, 2> samples{{{.EastM = edge.EastM - stepM * runN / lengthM,
                                           .NorthM = edge.NorthM + stepM * runE / lengthM},
                                          {.EastM = edge.EastM + stepM * runN / lengthM,
                                           .NorthM = edge.NorthM - stepM * runE / lengthM}}};
  std::array<double, 2> heights{10, 10};
  (void)ApplyEarthworkStamps(contacts, samples, heights, kMostEarthworkM);
  CHECK(std::abs(heights[0] - 10) < contactToleranceM,
        "the neighboring cut preserves the physical path contact");
  CHECK(std::abs(heights[1] - heights[0]) / (2 * stepM) < maximumSlope,
        "an adjacent earthwork cannot create a step outside an unchanged path");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  outshine::Generators::GroundMaterialCatalog materials;
  outshine::Generators::RegionSurfaceRecipes vegetation;
  CHECK(materials.Load("data/world/ground-materials.json") &&
            vegetation.Load("data/world/vegetation.json", materials),
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
  const Generators::ProfiledRoadMesher mesher;
  const Generators::Corridors corridors(mesher);
  Geometry geometry;
  std::vector<EarthworkStamp> contacts;
  std::vector<DiagnosticSample> notes;
  CHECK(corridors.Lay(site, geometry, contacts, notes), "the unchanged path is generated");
  CHECK(!contacts.empty(), "a ground surface publishes its physical core even without earthwork");
  for (const auto &contact : contacts) {
    CHECK(contact.ApronM > 0, "every ground path retains a finite transition at its boundary");
  }
  constexpr double cutHalfLengthM = 100;
  constexpr uint64_t neighborKey = 42;
  constexpr double contactToleranceM = 0.001;
  contacts.push_back(
      {.RingEastNorthM =
           {-cutHalfLengthM, 4, cutHalfLengthM, 4, cutHalfLengthM, 6, -cutHalfLengthM, 6},
       .HoleRingsEastNorthM = {},
       .LowE = -cutHalfLengthM,
       .HighE = cutHalfLengthM,
       .LowN = 4,
       .HighN = 6,
       .PlateauM = 8,
       .SeamEastNorthM = {},
       .Profile = {},
       .PlanarSupportM = {},
       .CorridorKey = neighborKey,
       .ApronM = 8,
       .YieldM = 2,
       .Fills = true,
       .Kind = EarthworkKind::Corridor});
  constexpr double coreOffsetM = 20;
  const std::array<EastNorth, 4> samples{{{.EastM = -coreOffsetM, .NorthM = 0},
                                          {.EastM = 0, .NorthM = 0},
                                          {.EastM = coreOffsetM, .NorthM = 0},
                                          {.EastM = 0, .NorthM = 3}}};
  for (size_t at = 0; at + 1 < contacts.size(); ++at) {
    CHECK(samples.back().NorthM > contacts[at].HighN,
          "the outside probe lies beyond the path's physical shoulder");
  }
  std::array<double, 4> heights{{10, 10, 10, 10}};
  (void)ApplyEarthworkStamps(contacts, samples, heights, kMostEarthworkM);
  for (size_t at = 0; at < 3; ++at) {
    CHECK(std::abs(heights[at] - 10) < contactToleranceM,
          "the neighboring cut apron cannot remove terrain under the unchanged path");
  }
  CHECK(heights.back() < 10 && heights.back() > 8,
        "the adjacent earthwork still deforms unoccupied terrain smoothly");
  CheckBoundaryContinuity(contacts);
  return Report();
}
