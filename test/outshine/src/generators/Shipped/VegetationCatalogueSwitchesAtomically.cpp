#include "Shipped.h"
#include "Check.h"
#include <cstdlib>
#include <filesystem>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::string directory =
      (std::filesystem::temp_directory_path() / "outshine-species-XXXXXX").string();
  const bool created = mkdtemp(directory.data()) != nullptr;
  CHECK(created, "isolated empty species directory created");
  if (!created) { return Report(); }
  outshine::Ground::GroundMaterials materials;
  outshine::Ground::VegetationTemplates vegetation;
  outshine::Ground::VegetationTemplates absent;
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "ground materials load");
  CHECK(vegetation.Load("src/assets/world/vegetation.json", materials),
        "vegetation templates load");
  Shipping catalogue;
  std::string error;
  const auto *groundMesher = &catalogue.Covering();
  const auto *roadMesher = &catalogue.Paving();
  for (int iteration = 0; iteration < 3; ++iteration) {
    CHECK(catalogue.Stands(absent, directory, error, false),
          "disabled vegetation needs neither templates nor species files");
    CHECK(catalogue.Ready() && catalogue.Placing().Count() == 1 &&
              catalogue.Drawing().Count() == 1 && catalogue.TreeFor(ClusterId{0}) == nullptr,
          "disabled catalogue retains building generation and drawing without tree prototypes");
    const auto *building = &catalogue.Placing().At(0);
    CHECK(!catalogue.Stands(vegetation, directory, error, true) && !error.empty(),
          "enabling from an empty species directory fails");
    CHECK(catalogue.Ready() && catalogue.Placing().Count() == 1 &&
              &catalogue.Placing().At(0) == building && catalogue.TreeFor(ClusterId{0}) == nullptr,
          "failed activation preserves the prior registry and borrowed generator");
    CHECK(catalogue.Stands(vegetation, "src/assets/world/species", error, true),
          "valid species activate after failed activation");
    CHECK(catalogue.Placing().Count() == 2 && catalogue.Drawing().Count() == 2 &&
              catalogue.TreeFor(ClusterId{0}) != nullptr,
          "activation restores flora generation and drawing");
    CHECK(&catalogue.Covering() == groundMesher && &catalogue.Paving() == roadMesher,
          "catalogue changes preserve independent terrain and infrastructure services");
  }
  CHECK(catalogue.Stands(absent, directory, error, false) &&
            catalogue.TreeFor(ClusterId{0}) == nullptr,
        "final deactivation releases species");
  std::filesystem::remove_all(directory);
  return Report();
}
