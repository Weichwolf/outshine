#include "VegetationTemplates.h"
#include "Check.h"

#include <cstddef>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  VegetationTemplates::Row row{
      .Ground = {{.2f, .1f, .05f, .9f}}, .Litter = {{.05f, .2f, .3f, .6f}}, .GroundClass = 7};
  CHECK(row.MeanGround() == row.Ground, "zero litter retains exact substrate appearance");
  row.Mix[0] = 1;
  CHECK(row.MeanGround() == row.Litter, "full litter retains exact covering appearance");
  row.Mix[0] = .75f;
  const Vec4f expected{{.0875f, .175f, .2375f, .675f}};
  const auto mixed = row.MeanGround();
  for (size_t channel = 0; channel < 4; ++channel) {
    CHECK_NEAR(mixed[channel], expected[channel], 1e-7, "declared coverage", "native mean layer");
  }
  CHECK(row.GroundClass == 7 && row.Ground[3] == .9f && row.Litter[3] == .6f,
        "appearance composition preserves substrate identity and source finishes");
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
