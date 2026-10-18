#include "PlantRecipeFixture.h"
#include "PlantRecipe.h"
#include "Check.h"
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

namespace {
bool Tracking = false;
size_t Allocations = 0;
}

void *operator new(std::size_t bytes) {
  void *memory = std::malloc(bytes == 0 ? 1 : bytes);
  if (memory == nullptr) { std::abort(); }
  if (Tracking) { ++Allocations; }
  return memory;
}

void operator delete(void *memory) noexcept {
  std::free(memory);
}

void operator delete(void *memory, [[maybe_unused]] std::size_t bytes) noexcept {
  std::free(memory);
}

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  auto recipe = PlantRecipeFixture();
  constexpr size_t kAxes = 10;
  constexpr size_t kLeaves = 32;
  PlantRecipeDemand demand{.MaximumAxes = kAxes, .MaximumLeaves = kLeaves};
  Tracking = true;
  const auto size = MeasurePlantRecipe(recipe, demand);
  Tracking = false;
  CHECK(size && Allocations == 0,
        "recipe preflight allocates neither axis state nor organ placements");
  --demand.MaximumAxes;
  Tracking = true;
  const auto shortAxes = BuildPlantPlan(recipe, demand);
  Tracking = false;
  CHECK(!shortAxes && Allocations == 0, "insufficient axis budget refuses before growth");
  ++demand.MaximumAxes;
  --demand.MaximumLeaves;
  Tracking = true;
  const auto shortLeaves = BuildPlantPlan(recipe, demand);
  Tracking = false;
  CHECK(!shortLeaves && Allocations == 0, "insufficient leaf budget refuses before growth");
  ++demand.MaximumLeaves;
  demand.Path[0] = 3;
  CHECK(!BuildPlantPlan(recipe, demand), "path outside source branching is refused");
  demand.Path = {-1, 0, -1, -1, -1, -1, -1};
  CHECK(!BuildPlantPlan(recipe, demand), "noncanonical path padding is refused");
  demand.Path.fill(-1);
  recipe.Levels[1].Branches = 0;
  CHECK(!BuildPlantPlan(recipe, demand), "zero-branch levels are invalid before any division");
  recipe = PlantRecipeFixture();
  recipe.Blades[1].PetioleM = 1;
  CHECK(!BuildPlantPlan(recipe, demand), "shared organ placement requires equal petioles");
  recipe = PlantRecipeFixture();
  recipe.Levels[1].BaseRadiusM = 1;
  CHECK(!BuildPlantPlan(recipe, demand), "child base must fit in its parent attachment radius");
  recipe = PlantRecipeFixture();
  recipe.Levels[1].LeafInternodeM = std::numeric_limits<double>::min();
  CHECK(!BuildPlantPlan(recipe, demand),
        "unrepresentable organ count cannot overflow IDs or allocate");
  return Report();
}
