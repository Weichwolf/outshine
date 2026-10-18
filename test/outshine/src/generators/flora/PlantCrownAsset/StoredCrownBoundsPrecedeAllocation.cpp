#include "CrownAssetFixture.h"
#include "PlantCrownAsset.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <span>
#include <vector>

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
  const auto source = CrownAssetFixture();
  if (!source) { return 1; }
  const auto encoded = EncodePlantCrownPlan(*source, kCrownAssetBudget);
  CHECK(encoded, "valid stored input is available before allocation tracking");
  if (!encoded) { return Report(); }
  Tracking = true;
  const auto underbudget = EncodePlantCrownPlan(*source, encoded->size() - 1);
  const auto oversized = DecodePlantCrownPlan(*encoded, encoded->size() - 1);
  Tracking = false;
  CHECK(!underbudget && !oversized && Allocations == 0,
        "both size refusals happen before allocating an encoded or decoded product");
  for (size_t prefix = 0; prefix < encoded->size(); ++prefix) {
    Tracking = true;
    const auto shortInput =
        DecodePlantCrownPlan(std::span(*encoded).first(prefix), kCrownAssetBudget);
    Tracking = false;
    CHECK(!shortInput && Allocations == 0, "every truncated prefix fails before array allocation");
  }
  Tracking = true;
  const auto restored = DecodePlantCrownPlan(*encoded, kCrownAssetBudget);
  Tracking = false;
  constexpr size_t kPayloadArrayBytes = 169;
  CHECK(restored && Allocations == 5 && Bytes == kPayloadArrayBytes,
        "decode allocates exactly five stored arrays, no distance workspace, growth or full leaf "
        "poses");
  return Report();
}
