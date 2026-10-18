#include "PlantRecipeJsonFixture.h"
#include "PlantRecipeJson.h"
#include "PlantRecipe.h"
#include "Check.h"
#include <cstdint>
#include <limits>
#include <string>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto recipe = ReadPlantRecipe(kPlantRecipeJson, kPlantRecipeJson.size());
  CHECK(recipe && recipe->Levels.size() == 1 && recipe->Blades.size() == 1,
        "complete canonical v2 recipe loads without native-only wrapper fields");
  if (!recipe) { return Report(); }
  const auto plan = BuildPlantPlan(*recipe, {.MaximumAxes = 1, .MaximumLeaves = 7});
  CHECK(plan && plan->Summarize(0)->Axes == 1 && plan->Summarize(0)->Leaves == 7,
        "loaded recipe drives bounded native growth rather than stored Python poses");
  std::string high(kPlantRecipeJson);
  high.replace(high.find("\"seed\": 17"),
               std::string_view("\"seed\": 17").size(),
               "\"seed\": 18446744073709551615");
  const auto exact = ReadPlantRecipe(high, high.size());
  CHECK(exact && exact->Seed == std::numeric_limits<uint64_t>::max(),
        "seed comes from integer source digits without Float64 loss");
  std::string extended(kPlantRecipeJson);
  extended.replace(extended.find("branching-plant-2"),
                   std::string_view("branching-plant-2").size(),
                   "branching-plant-3");
  const std::string_view arrangement = R"("arrangement": "spiral")";
  extended.replace(extended.find(arrangement),
                   arrangement.size(),
                   R"("arrangement": "whorled", "whorl_size": 3, "angle_tip": 0.8)");
  const auto whorled = ReadPlantRecipe(extended, extended.size());
  CHECK(whorled && whorled->Levels.front().WhorlSize == 3 && whorled->Levels.front().TipAngleRad &&
            whorled->Levels.front().Arrangement == PlantArrangement::Whorled,
        "v3 branch controls and leaf arrangement remain independent declared parameters");
  CHECK(whorled && BuildPlantPlan(*whorled, {.MaximumAxes = 1, .MaximumLeaves = 21}),
        "extended canonical recipe is usable by the same native planner");
  return Report();
}
