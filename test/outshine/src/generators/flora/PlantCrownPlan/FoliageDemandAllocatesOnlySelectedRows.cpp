#include "PlantCrownPlan.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <utility>
#include <vector>

namespace {
bool Tracking = false;
size_t Allocations = 0, Bytes = 0;

outshine::Generators::PlantGrowthPlan Skeleton() {
  return {.PositionsM = {{0, 0, 0}, {0, 0, 1}, {0, 0, 2}, {1, 0, 2}, {0, 0, 3}},
          .Parents = {-1, 0, 1, 2, 2},
          .Unreached = {},
          .Stats = {}};
}
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
  constexpr double kLengthM = 0.35;
  const PlantCrownRecipe recipe{.LengthM = kLengthM, .FoliageDepthM = 1};
  constexpr size_t kLeaves = 90000;
  auto source = Skeleton();
  std::vector<uint8_t> variants(kLeaves, 1);
  Tracking = true;
  const auto rejected = PlantCrownPlan::Create(std::move(source), std::move(variants), recipe, 59);
  Tracking = false;
  CHECK(!rejected && Allocations == 0,
        "index budget refusal allocates nothing and emits no leaf poses");
  source = Skeleton();
  variants.assign(kLeaves, 1);
  Tracking = true;
  const auto plan = PlantCrownPlan::Create(std::move(source), std::move(variants), recipe, 60);
  Tracking = false;
  CHECK(plan && Allocations == 2 && Bytes == 60,
        "ninety thousand leaves add only two node-sized index arrays");
  if (!plan) { return Report(); }
  Allocations = Bytes = 0;
  Tracking = true;
  const auto count = plan->MeasureLeaves({.Node = 3, .Maximum = 30000});
  const auto overbudget = plan->Leaves({.Node = 3, .Maximum = 29999});
  Tracking = false;
  CHECK(count && *count == 30000 && !overbudget && Allocations == 0,
        "leaf counting and insufficient selected demand never allocate poses");
  Allocations = Bytes = 0;
  Tracking = true;
  const auto group = plan->Leaves({.Node = 3, .Maximum = 30000});
  Tracking = false;
  CHECK(group && group->size() == 30000 && Allocations == 1 &&
            Bytes == 30000 * sizeof(PlantCrownLeaf) && group->front().Id == 30000 &&
            group->back().Id == 59999,
        "one requested group allocates exactly its own rows, leaving every other host unexpanded");
  return Report();
}
