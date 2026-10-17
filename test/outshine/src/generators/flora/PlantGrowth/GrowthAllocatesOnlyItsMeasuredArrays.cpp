#include "PlantGrowth.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace {
bool Tracking = false;
size_t Allocations = 0, Bytes = 0;
}

void *operator new(std::size_t bytes) {
  void *memory = std::malloc(bytes == 0 ? 1 : bytes);
  if (memory == nullptr) { std::abort(); }
  if (Tracking) {
    ++Allocations;
    Bytes += bytes;
  }
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
  constexpr std::array<PlantGrowthPoint, 2> kSeed{PlantGrowthPoint{0, 0, 0}, {0, 0, 1}};
  constexpr std::array<int32_t, 2> kParents{-1, 0};
  constexpr std::array<PlantGrowthPoint, 2> kPoints{PlantGrowthPoint{-1, 0, 3}, {1, 0, 3}};
  constexpr double kStepM = 0.1;
  constexpr double kKillM = 0.15;
  PlantGrowthDemand demand{.Nodes = 64, .StepM = kStepM, .InfluenceM = 10, .KillM = kKillM};
  Tracking = true;
  const auto measured = MeasurePlantGrowth(kPoints, kSeed, kParents, demand);
  Tracking = false;
  CHECK(measured && Allocations == 0 && Bytes == 0, "valid growth preflight allocates nothing");
  if (!measured) { return Report(); }
  demand.WorkingBytes = *measured - 1;
  Tracking = true;
  const auto rejected = GrowPlant(kPoints, kSeed, kParents, demand);
  Tracking = false;
  CHECK(!rejected && Allocations == 0 && Bytes == 0,
        "underbudget growth refuses before any allocation");
  demand.WorkingBytes = *measured;
  Tracking = true;
  const auto plan = GrowPlant(kPoints, kSeed, kParents, demand);
  Tracking = false;
  CHECK(plan && Allocations == 9 && Bytes == *measured && plan->Stats.WorkingBytes == *measured &&
            plan->Stats.NearestQueries > 1,
        "complete growth, both indices and all query iterations use exactly the nine measured "
        "arrays");
  return Report();
}
