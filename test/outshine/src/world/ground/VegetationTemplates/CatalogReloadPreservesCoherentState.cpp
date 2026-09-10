#include "VegetationTemplates.h"
#include "GroundMaterials.h"
#include "Check.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  auto temporary = (std::filesystem::temp_directory_path() / "outshine-vegetation-XXXXXX").string();
  const char *created = mkdtemp(temporary.data());
  CHECK(created != nullptr, "temporary directory created");
  if (!created) { return Report(); }
  const auto materialsPath = std::filesystem::path(created) / "materials.json";
  const auto catalogPath = std::filesystem::path(created) / "vegetation.json";
  const auto materialText = [](const char *friction) {
    return std::string(
               R"({"frictionModel":{"reference":"reference"},"classes":[{"name":"reference","peakFriction":0.5,"surface":"coherent","slope":{"plausibleDeg":[0,90]}},{"name":"road","surface":"coherent","slope":{"plausibleDeg":[0,90]},"peakFriction":)") +
           friction + "}]}";
  };
  const std::string catalog =
      R"({"bladeClasses":[{"name":"blade","greenLinear":[0.1,0.2,0.3],"dryLinear":[0.3,0.2,0.1]}],"templates":[{"name":"road","ground":{"class":"road"},"grass":{"class":"blade"},"osm":[{"layer":"streets","kind":"road","rank":1,"widthM":2}]}],"unmapped":{"class":"reference"},"alpineLimit":{"rockTemplate":"road"},"waterClearance":[{"runM":1,"clearanceM":2}]})";
  std::ofstream(materialsPath) << materialText("1");
  std::ofstream(catalogPath) << catalog;
  GroundMaterials materials;
  VegetationTemplates templates;
  CHECK(materials.Load(materialsPath.c_str()), materials.Error().c_str());
  CHECK(templates.Load(catalogPath.c_str(), materials), templates.Error().c_str());
  if (!templates.Ready()) {
    std::filesystem::remove_all(created);
    return Report();
  }
  CHECK(templates.TemplateCount() == 2 && templates.FrictionOf(0) == 2,
        "initial friction is relative to reference");
  const auto *rows = templates.Rows();
  const auto *rule = templates.Find("streets", "road");
  for (const std::string &invalid : {std::string("{}"),
                                     std::string("{"),
                                     catalog.substr(0, catalog.find("\"alpineLimit\"")) +
                                         "\"alpineLimit\":{\"rockTemplate\":\"missing\"}}"}) {
    std::ofstream(catalogPath) << invalid;
    CHECK(!templates.Load(catalogPath.c_str(), materials), "invalid replacement rejected");
    CHECK(templates.Ready() && templates.Rows() == rows && templates.TemplateCount() == 2,
          "failed replacement retains row storage");
    CHECK(templates.Find("streets", "road") == rule && templates.FrictionOf(0) == 2,
          "failed replacement retains rule and friction");
  }
  CHECK(!templates.Load((std::filesystem::path(created) / "missing").c_str(), materials),
        "missing file refused");
  CHECK(templates.Rows() == rows && templates.TemplateCount() == 2,
        "IO failure preserves snapshot");
  std::ofstream(catalogPath) << catalog << std::string(1024u * 1024u, ' ');
  CHECK(!templates.Load(catalogPath.c_str(), materials),
        "oversized valid JSON refused before parsing");
  CHECK(templates.Rows() == rows, "size-budget failure preserves snapshot");
  CHECK(!templates.Load(nullptr, materials) && templates.Rows() == rows,
        "null path safely preserves snapshot");
  std::ofstream(materialsPath) << materialText("0.25");
  std::ofstream(catalogPath) << catalog;
  CHECK(materials.Load(materialsPath.c_str()), "replacement material fixture loads");
  CHECK(templates.Load(catalogPath.c_str(), materials), "valid replacement succeeds");
  CHECK(templates.FrictionOf(0) == 0.5f && templates.FrictionOf(1) == 1 &&
            templates.FrictionOf(2) == 0,
        "reload replaces friction rows instead of appending to old values");
  CHECK(templates.Error().empty() && templates.WaterBands().size() == 1,
        "success clears error and replaces auxiliary data");
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            templates.Load("src/assets/world/vegetation.json", materials),
        "shipped catalogs remain accepted");
  std::filesystem::remove_all(created);
  return Report();
}
