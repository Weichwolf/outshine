#include "VegetationTemplates.h"
#include "GroundMaterials.h"
#include "Check.h"
#include <cstddef>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  GroundMaterials materials;
  VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "shipped material catalog");
  CHECK(templates.Load("src/assets/world/vegetation.json", materials), "shipped surface recipes");
  if (!templates.Ready()) { return Report(); }
  for (size_t at = 0; at < templates.TemplateCount(); ++at) {
    const auto &source = templates.Rows()[at];
    const auto appearance = source.MeanGround();
    if (source.Mix[0] == 0) {
      CHECK(appearance == source.Ground, "uncovered recipes retain their original optical values");
    }
    for (const float value : appearance) {
      CHECK(value >= 0 && value <= 1, "shipped mean reflectance and roughness remain fractions");
    }
  }
  return Report();
}
