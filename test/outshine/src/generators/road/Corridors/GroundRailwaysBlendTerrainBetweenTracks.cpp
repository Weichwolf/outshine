#include "Check.h"
#include "Corridors.h"
#include "EarthworkPress.h"
#include "ProfiledRoadMesher.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using Sampler = std::function<std::optional<double>(EastNorth)>;

std::vector<EarthworkStamp> Contacts(bool bridge, Sampler sample) {
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "the production material and traffic recipes load");
  const std::array<std::string, 1> layers{"streets"};
  Generators::Osm::OsmField vectors(14, layers);
  const std::array declared{
      Generators::Osm::OsmField::Declared{.Layer = "streets",
                                          .Key = "kind",
                                          .Value = "rail",
                                          .Bridge = bridge,
                                          .LatLon = {49, 7.999, 49, 8.001}},
      Generators::Osm::OsmField::Declared{
          .Layer = "streets",
          .Key = "kind",
          .Value = "rail",
          .Bridge = bridge,
          .LatLon = {49 + 12.0 / kMPerDegLat, 7.999, 49 + 12.0 / kMPerDegLat, 8.001}}};
  CHECK(vectors.Declare(declared, origin).has_value(), "two parallel railway axes are declared");
  Generators::Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, vegetation) == 2, "both source railways are retained");
  CHECK(std::ranges::all_of(ways.Ways(),
                            [](const auto &way) {
                              return way.TrafficKind == Generators::Osm::StreetField::Traffic::Rail;
                            }),
        "the test exercises the normalized railway classification");
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty, .Field = std::move(sample)};
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
  Geometry geometry;
  std::vector<EarthworkStamp> contacts;
  std::vector<DiagnosticSample> notes;
  CHECK(Generators::Corridors(mesher).Lay(site, geometry, contacts, notes),
        "the production road mesher builds both railway ribbons");
  CHECK(geometry.parts() > 0, "renderable railway geometry is retained");
  return contacts;
}

std::optional<double> Terrain(EastNorth at) {
  const double distance = std::min(std::abs(at.NorthM), std::abs(at.NorthM - 12.0));
  const double t = std::clamp((distance - 3.5) / 2.0, 0.0, 1.0);
  return 10.0 + 0.8 * t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto contacts = Contacts(false, Terrain);
  CHECK(!contacts.empty(), "ground railways publish finite terrain contacts");
  CHECK(std::ranges::all_of(
            contacts,
            [](const auto &stamp) { return stamp.ApronM > 0 && stamp.ApronM <= kMostApronM; }),
        "ground railway shoulders remain bounded even when axis and edge samples agree");
  const std::array<EastNorth, 5> points{{{0, 0}, {0, 6}, {0, 12}, {0, -25}, {0, 37}}};
  std::array<double, 5> heights;
  for (size_t at = 0; at < points.size(); ++at) { heights[at] = *Terrain(points[at]); }
  const auto result = ApplyEarthworkStamps(contacts, points, heights, kMostEarthworkM);
  CHECK(result.Refused.empty() || std::ranges::none_of(result.Refused, [](auto v) { return v; }),
        "compatible parallel track beds are not discarded");
  CHECK(std::abs(heights[0] - 10) < 0.001 && std::abs(heights[2] - 10) < 0.001,
        "the physical railway heights remain fixed");
  CHECK(heights[1] >= 10 && heights[1] < 10.1,
        "nearby compatible track beds do not leave a thin source-height ridge between them");
  CHECK(std::abs(heights[3] - *Terrain(points[3])) < 0.001 &&
            std::abs(heights[4] - *Terrain(points[4])) < 0.001,
        "terrain outside the finite construction neighborhood is unchanged");
  const auto flat = Contacts(false, [](EastNorth) -> std::optional<double> { return 10; });
  heights.fill(10);
  (void)ApplyEarthworkStamps(flat, points, heights, kMostEarthworkM);
  CHECK(std::ranges::all_of(heights, [](double h) { return std::abs(h - 10) < 0.001; }),
        "a matching flat source remains flat inside and outside railway footprints");
  const auto bridge = Contacts(true, Terrain);
  CHECK(std::ranges::all_of(bridge,
                            [](const auto &stamp) {
                              return stamp.Kind == EarthworkKind::Clearance && !stamp.Fills;
                            }),
        "elevated railways retain clearance contacts instead of filling the ground");
  return Report();
}
