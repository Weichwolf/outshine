#include "PreparedPlantBuildFixture.h"
#include "BuildPreparedPlant.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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
  const auto plan = PlantBuildPlan();
  CHECK(plan, "prepared source plan is available before tracking");
  if (!plan) { return Report(); }
  const auto blades = PlantBuildBlades();
  const auto materials = PlantBuildMaterials();
  const auto demand = PlantBuildDemand();
  Tracking = true;
  const auto counts = MeasurePreparedPlant(*plan, blades, demand);
  Tracking = false;
  CHECK(counts && Allocations == 0,
        "full preflight measures without allocation or per-leaf mesh work");
  const std::array<uint8_t, 32> identity{};
  const std::array<size_t PlantProductCounts::*, 4> limits{&PlantProductCounts::Vertices,
                                                           &PlantProductCounts::Triangles,
                                                           &PlantProductCounts::LeafInstances,
                                                           &PlantProductCounts::TriangleReferences};
  for (const auto limit : limits) {
    auto shortDemand = demand;
    --(shortDemand.Maximum.*limit);
    Allocations = 0;
    Tracking = true;
    const auto measured = MeasurePreparedPlant(*plan, blades, shortDemand);
    const auto built = BuildPreparedPlant(*plan, blades, identity, materials, shortDemand);
    Tracking = false;
    CHECK(!measured && !built && Allocations == 0,
          "each independent underbudget refuses the entire product before any allocation");
  }
  auto invalid = demand;
  invalid.Root = kUnknown;
  CHECK(!BuildPreparedPlant(*plan, blades, identity, materials, invalid),
        "unknown branch is not replaced by root");
  invalid = demand;
  invalid.AxisSides = 0;
  CHECK(!BuildPreparedPlant(*plan, blades, identity, materials, invalid),
        "invalid surface demand is not silently repaired");
  CHECK(!BuildPreparedPlant(*plan, {}, identity, materials, demand),
        "variant catalogue must match source-plan ownership");
  return Report();
}
