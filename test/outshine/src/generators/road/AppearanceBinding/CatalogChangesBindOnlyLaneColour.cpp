#include "RoadLaneColour.h"
#include "RegionSurfaceRecipes.h"
#include "GroundMaterialCatalog.h"
#include "RoadPageAppearance.h"
#include "RoadPageAsset.h"
#include "Check.h"
#include "Earth.h"
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <unistd.h>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr LongitudeLatitude kAnchor{.LongitudeDeg = 8, .LatitudeDeg = 49};

std::string
Catalog(const std::string &ground, const std::string &blade, const std::string &density) {
  return R"({"bladeClasses":[{"name":"a","greenLinear":[0.1,0.2,0.3],"dryLinear":[0.3,0.2,0.1]},{"name":"b","greenLinear":[0.3,0.1,0.2],"dryLinear":[0.2,0.3,0.1]}],"templates":[{"name":"road","ground":{"class":")" +
         ground + R"("},"grass":{"class":")" + blade + R"(","perM2":)" + density +
         R"(},"osm":[{"layer":"streets","kind":"road","rank":1,"widthM":2}]}],"unmapped":{"class":"asphalt"},"alpineLimit":{"rockTemplate":"road"},"waterClearance":[{"runM":1,"clearanceM":2}]})";
}

std::string Key(const RegionSurfaceRecipes &templates) {
  const std::array bindings{
      RoadPagePartAppearance{.Part = {.Kind = RoadPagePartKind::Interval, .Index = 0},
                             .WearsLinear = RoadLaneColour(0, templates)}};
  return RoadPageKey({.PlanKey = std::string(64, 'a'),
                      .MaterialKey = std::string(64, 'b'),
                      .MaterialIndex = 0,
                      .Anchor = kAnchor,
                      .Level = 0,
                      .Parts = {bindings[0].Part},
                      .Appearance = RoadPageAppearanceV2{RoadPageAppearanceKey(bindings)}});
}
}

int main() {
  const auto directory = std::filesystem::temp_directory_path() /
                         ("outshine-lane-appearance-" + std::to_string(getpid()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "the surface catalogue contrast owns its temporary inputs");
  if (error) { return Report(); }
  const auto path = directory / "vegetation.json";
  GroundMaterialCatalog materials;
  RegionSurfaceRecipes templates;
  CHECK(materials.Load("data/world/ground-materials.json"),
        "the shipped native material catalogue loads");
  std::ofstream(path) << Catalog("asphalt", "a", "1");
  CHECK(templates.Load(path.c_str(), materials), templates.Error().c_str());
  if (!templates.Ready()) {
    std::filesystem::remove_all(directory, error);
    return Report();
  }
  const auto initial = Key(templates);
  const auto initialColour = templates.Rows()[0].Ground;
  const auto initialGrass = templates.Rows()[0].Grass;
  std::ofstream(path) << Catalog("asphalt", "b", "2");
  CHECK(templates.Load(path.c_str(), materials), templates.Error().c_str());
  CHECK(templates.Ready() && templates.Rows()[0].Ground == initialColour &&
            templates.Rows()[0].Grass != initialGrass && !initial.empty() &&
            Key(templates) == initial,
        "real catalogue species and density changes retain the exact consumed LaneColour page "
        "binding");
  std::ofstream(path) << Catalog("gravel", "b", "2");
  CHECK(templates.Load(path.c_str(), materials), templates.Error().c_str());
  CHECK(templates.Ready() && templates.Rows()[0].Ground != initialColour &&
            Key(templates) != initial,
        "real native LaneColour changes invalidate the render page independently of plan and PBR "
        "keys");
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the catalogue contrast cleans its private source files");
  return Report();
}
