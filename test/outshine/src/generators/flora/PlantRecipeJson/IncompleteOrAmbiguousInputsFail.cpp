#include "PlantRecipeJsonFixture.h"
#include "PlantRecipeJson.h"
#include "Check.h"
#include <cstddef>
#include <array>
#include <string>
#include <string_view>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  CHECK(!ReadPlantRecipe(kPlantRecipeJson, kPlantRecipeJson.size() - 1),
        "input byte budget applies before parsing the document");
  for (size_t size = 0; size < kPlantRecipeJson.size(); ++size) {
    CHECK(!ReadPlantRecipe(kPlantRecipeJson.substr(0, size), kPlantRecipeJson.size()),
          "truncated canonical input cannot publish a partially defaulted recipe");
  }
  constexpr std::array kSeeds{"-1", "17.0", "17e0", "true", "\"17\"", "18446744073709551616"};
  const std::string_view seed = "\"seed\": 17";
  for (const auto *value : kSeeds) {
    std::string changed(kPlantRecipeJson);
    changed.replace(changed.find(seed), seed.size(), std::string("\"seed\": ") + value);
    CHECK(!ReadPlantRecipe(changed, changed.size()),
          "canonical seed must be an exact unsigned64-bit JSON integer token");
  }
  constexpr std::array kChanges{
      std::array<std::string_view, 2>{"\"base_radius\": 0.019", "\"base_radius\": -0.019"},
      std::array<std::string_view, 2>{"\"length\": 1.55", "\"length\": true"},
      std::array<std::string_view, 2>{"\"leaf_internode\": 0.2", R"("leaf_internode": "0.2")"},
      std::array<std::string_view, 2>{R"("arrangement": "spiral")", R"("arrangement": "unknown")"},
      std::array<std::string_view, 2>{"\"branches\": 1", "\"branches\": 0"},
      std::array<std::string_view, 2>{"\"seed\": 17", R"("seed": 17, "seed": 18)"},
      std::array<std::string_view, 2>{"branching-plant-2", "branching-plant-4"},
      std::array<std::string_view, 2>{"\"tip_scale\": 1", "\"unknown\": 1"},
  };
  for (const auto &change : kChanges) {
    std::string changed(kPlantRecipeJson);
    const auto at = changed.find(change[0]);
    CHECK(at != std::string::npos, "negative control mutates an actual canonical field");
    if (at == std::string::npos) { return Report(); }
    changed.replace(at, change[0].size(), change[1]);
    CHECK(!ReadPlantRecipe(changed, changed.size()),
          "wrong types, versions, unknown/missing fields and invalid geometry are rejected");
  }
  return Report();
}
