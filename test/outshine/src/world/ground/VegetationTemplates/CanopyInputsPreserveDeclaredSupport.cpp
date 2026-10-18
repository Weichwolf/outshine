#include "VegetationTemplates.h"
#include "GroundMaterials.h"
#include "Check.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  auto temporary = (std::filesystem::temp_directory_path() / "outshine-canopy-XXXXXX").string();
  const char *created = mkdtemp(temporary.data());
  CHECK(created != nullptr, "temporary directory created");
  if (!created) { return Report(); }
  const auto path = std::filesystem::path(created) / "vegetation.json";
  const std::string catalog =
      R"({"bladeClasses":[{"name":"blade","greenLinear":[0.1,0.2,0.3],"dryLinear":[0.3,0.2,0.1]}],"templates":[{"name":"stand","osm":[{"layer":"land","kind":"grass","rank":1}],"ground":{"class":"limestone","swardClosure":0.75},"grass":{"class":"blade","perM2":100,"heightM":0.12,"widthM":0.01,"heightJitter":0.5,"dryFraction":0.25}}],"unmapped":{"class":"limestone"},"alpineLimit":{"rockTemplate":"stand"}})";
  GroundMaterials materials;
  VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "shipped substrate catalog");
  const auto load = [&](const std::string &text) {
    std::ofstream(path) << text;
    return templates.Load(path.c_str(), materials);
  };
  CHECK(load(catalog), templates.Error().c_str());
  if (!templates.Ready()) {
    std::filesystem::remove_all(created);
    return Report();
  }
  const auto *rows = templates.Rows();
  const auto original = rows[0];
  CHECK(original.CanopyWeight == 0.75f, "declared coarse material weight retained");
  for (size_t channel = 0; channel < 3; ++channel) {
    const float canopy = original.Grass[channel] * 0.75f + original.Dry[channel] * 0.25f;
    CHECK_NEAR(original.Ground[channel],
               original.GroundBase[channel] * 0.25f + canopy * 0.75f,
               1e-7,
               "coarse substrate mean",
               "raw substrate remains recoverable");
    CHECK_NEAR(original.Litter[channel],
               original.LitterBase[channel] * 0.25f + canopy * 0.75f,
               1e-7,
               "coarse litter mean",
               "raw litter remains recoverable");
  }
  const std::array fields{
      "swardClosure", "perM2", "heightM", "widthM", "heightJitter", "dryFraction"};
  const std::array values{"0.75", "100", "0.12", "0.01", "0.5", "0.25"};
  const auto replaced = [&](size_t field, const char *value) {
    auto text = catalog;
    const auto old = "\"" + std::string(fields[field]) + "\":" + values[field];
    text.replace(text.find(old), old.size(), "\"" + std::string(fields[field]) + "\":" + value);
    return text;
  };
  for (size_t field = 0; field < fields.size(); ++field) {
    for (const auto *value : {"null", "true", "\"0.5\"", "-0.1", "1e100", "1e-100"}) {
      CHECK(!load(replaced(field, value)), "invalid declared canopy input refused");
      CHECK(templates.Rows() == rows && templates.Rows()[0].CanopyWeight == 0.75f,
            "failed reload preserves the complete published stand");
    }
    if (field == 0 || field >= 4) {
      CHECK(!load(replaced(field, "1.000000001")), "fraction checked before float rounding");
      CHECK(templates.Rows() == rows, "out-of-range fraction preserves snapshot");
    }
  }
  for (size_t field = 0; field < 4; ++field) {
    CHECK(load(replaced(field, "0")), "zero closure or support is a valid empty layer");
    const auto &empty = templates.Rows()[0];
    CHECK(empty.CanopyWeight == 0.0f, "absent stand contributes no material coat");
    for (size_t channel = 0; channel < 4; ++channel) {
      CHECK(empty.Ground[channel] == empty.GroundBase[channel] &&
                empty.Litter[channel] == empty.LitterBase[channel],
            "zero support preserves original substrate, litter and roughness exactly");
    }
  }
  CHECK(templates.Rows()[1].CanopyWeight == 0.0f, "unmapped substrate never grows a canopy");
  std::filesystem::remove_all(created);
  return Report();
}
