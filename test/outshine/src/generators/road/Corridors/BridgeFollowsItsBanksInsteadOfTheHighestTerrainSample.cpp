#include "src/generators/road/Corridors.h"
#include "src/generators/road/ProfiledRoadMesher.h"
#include "Check.h"
#include <math/RenderFrame.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
std::optional<float> HeightAt(const outshine::Geometry &geometry, float northM) {
  std::optional<float> highest;
  for (int part = 0; part < geometry.parts(); ++part) {
    const auto mesh =
        outshine::TriangleBvh::Over(geometry.positionsOf(part), geometry.trianglesOf(part));
    if (const auto height = mesh.Under(0, outshine::RenderFrame::ZOfNorth(northM))) {
      highest = highest ? std::max(*highest, *height) : *height;
    }
  }
  return highest;
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
        "the bridge uses production clearance rules");
  if (!vegetation.Ready()) { return Report(); }
  int water = -1;
  for (size_t row = 0; row < vegetation.TemplateCount(); ++row) {
    if (vegetation.Rows()[row].GroundClass == materials.Find("water")) {
      water = static_cast<int>(row);
      break;
    }
  }
  CHECK(water >= 0, "the fixture has a water classification");
  if (water < 0) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  Generators::Osm::OsmField vectors(14, layers);
  const std::array declared{
      Generators::Osm::OsmField::Declared{.Layer = "streets",
                                          .Key = "kind",
                                          .Value = "residential",
                                          .Bridge = true,
                                          .Level = 1,
                                          .LatLon = {48.999, 8, 49, 8, 49.001, 8}}};
  CHECK(vectors.Declare(declared, origin).has_value(), "the bridge crosses a water body");
  Generators::Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, vegetation) == 1, "the bridge reaches the road generator");
  const TangentFrame standing = TangentFrame::At(origin);
  auto grid = std::make_shared<ClassStructure::Grid>();
  grid->W = grid->H = 1;
  grid->OrgE = grid->OrgN = -500;
  grid->CellM = 1000;
  grid->Cells = {static_cast<uint32_t>(water) | (1u << 8), 0};
  const std::shared_ptr<const ClassStructure> classes = std::make_shared<ClassStructure>(
      standing, grid, std::make_shared<const ClassStructure::Grid>(), ClassStructure::FromRun{});
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty, .Field = [](EastNorth at) -> std::optional<double> {
                      return 20 + .05 * at.NorthM + (std::abs(at.NorthM) < 10 ? 60 : 0);
                    }};
  const Generators::ProfiledRoadMesher mesher;
  const Generators::Corridors corridors(mesher);
  for (const auto level : {std::optional<double>{}, std::optional<double>{0}}) {
    const Generators::Corridors::Site site{.Vectors = &vectors,
                                           .Ways = ways,
                                           .Materials = materials,
                                           .Vegetation = vegetation,
                                           .Standing = standing,
                                           .Draped = drape,
                                           .Classes = classes,
                                           .WaterUpM = [level](LongitudeLatitude) { return level; },
                                           .EyeLatDeg = origin.LatitudeDeg,
                                           .EyeLonDeg = origin.LongitudeDeg,
                                           .Projection = {.FocalPx = 800}};
    Geometry geometry;
    std::vector<EarthworkStamp> earthworks;
    std::vector<DiagnosticSample> notes;
    CHECK(corridors.Lay(site, geometry, earthworks, notes) && geometry.wellFormed(),
          "the bridge generates valid native geometry");
    for (const float northM : {-100.f, 0.f, 100.f}) {
      const auto height = HeightAt(geometry, northM);
      CHECK(height && std::abs(*height - (20 + .05 * northM)) < .02,
            "the deck joins different bank heights without copying an interior DEM obstruction "
            "or levelling every station to the higher bank");
    }
  }
  return Report();
}
