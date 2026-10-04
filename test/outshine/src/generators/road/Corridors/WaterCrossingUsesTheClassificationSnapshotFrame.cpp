#include "src/generators/road/Corridors.h"
#include "src/generators/road/ProfiledRoadMesher.h"
#include "Check.h"
#include "EarthworkPress.h"

#include <algorithm>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
double Measure(const std::vector<outshine::DiagnosticSample> &notes, std::string_view name) {
  for (const auto &note : notes) {
    if (note.Name == name) { return note.Value; }
  }
  return -1;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "production water and bridge rules are available");
  if (!vegetation.Ready()) { return Report(); }
  int water = -1;
  for (size_t row = 0; row < vegetation.TemplateCount(); ++row) {
    if (vegetation.Rows()[row].GroundClass == materials.Find("water")) {
      water = static_cast<int>(row);
      break;
    }
  }
  CHECK(water >= 0, "the classification table identifies water");
  if (water < 0) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  ::outshine::Generators::Osm::OsmField vectors(14, layers);
  const std::array<::outshine::Generators::Osm::OsmField::Declared, 3> declared{
      {{.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .Bridge = true,
        .Level = 1,
        .LatLon = {48.999, 8, 49.0, 8}},
       {.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .Bridge = true,
        .Level = 1,
        .LatLon = {49.0, 8, 49.001, 8}},
       {.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .Bridge = true,
        .Level = 1,
        .LatLon = {49.0, 8, 49.0007, 8.0008}}}};
  CHECK(vectors.Declare(declared, origin).has_value(), "a north-south bridge is declared");
  ::outshine::Generators::Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, vegetation) == 3, "the bridge reaches the native road input");
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty,
                    .Field = [](EastNorth) -> std::optional<double> { return 0; }};
  const TangentFrame standing = TangentFrame::At(origin);
  const Generators::ProfiledRoadMesher mesher;
  const Generators::Corridors corridors(mesher);
  for (const double longitude : {8.1, 9.0}) {
    auto grid = std::make_shared<ClassStructure::Grid>();
    grid->W = grid->H = 1;
    grid->OrgE = -8000;
    grid->OrgN = -1000;
    grid->CellM = 2000;
    grid->Cells = {static_cast<uint32_t>(water) | (1u << 8), 0};
    const std::shared_ptr<const ClassStructure> classes = std::make_shared<ClassStructure>(
        TangentFrame::At({.LongitudeDeg = longitude, .LatitudeDeg = 49}),
        grid,
        std::make_shared<const ClassStructure::Grid>(),
        ClassStructure::FromRun{});
    const Generators::Corridors::Site site{.Vectors = &vectors,
                                           .Ways = ways,
                                           .Materials = materials,
                                           .Vegetation = vegetation,
                                           .Standing = standing,
                                           .Draped = drape,
                                           .Classes = classes,
                                           .EyeLatDeg = origin.LatitudeDeg,
                                           .EyeLonDeg = origin.LongitudeDeg,
                                           .Projection = {.FocalPx = 800}};
    Geometry geometry;
    std::vector<EarthworkStamp> earthworks;
    std::vector<DiagnosticSample> notes;
    CHECK(corridors.Lay(site, geometry, earthworks, notes) && geometry.wellFormed(),
          "a native classification snapshot is sufficient to generate the bridge");
    CHECK(Measure(notes, "streets: junctions shaped") == 1,
          "the fixture contains an elevated three-way junction");
    if (longitude == 8.1) {
      CHECK(!earthworks.empty() && std::ranges::all_of(earthworks,
                                                       [](const auto &stamp) {
                                                         return stamp.Kind ==
                                                                EarthworkKind::Clearance;
                                                       }),
            "an elevated span emits clearance rather than a soil contact");
      EarthworkStamp basin;
      basin.Kind = EarthworkKind::Basin;
      basin.RingEastNorthM = {-100, -200, 100, -200, 100, 200, -100, 200};
      basin.LowE = -100;
      basin.HighE = 100;
      basin.LowN = -200;
      basin.HighN = 200;
      basin.PlateauM = -2;
      earthworks.push_back(std::move(basin));
      const std::array<EastNorth, 1> points{{{.EastM = 0, .NorthM = 0}}};
      std::array<double, 1> heights{0};
      (void)ApplyEarthworkStamps(earthworks, points, heights, 80.0);
      CHECK(heights[0] == -2, "the generated bridge leaves the basin floor below its deck");
    }
    CHECK(Measure(notes, "streets: stations under a bridge asked") > 0,
          "the generated bridge samples its water crossing");
    const double wet = Measure(notes, "streets: and of those, water");
    CHECK(longitude == 8.1 ? wet > 0 : wet == 0,
          "water follows the snapshot's geographic frame rather than the road's local frame");
  }
  return Report();
}
