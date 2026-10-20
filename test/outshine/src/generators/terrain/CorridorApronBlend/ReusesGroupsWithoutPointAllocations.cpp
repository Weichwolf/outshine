#include "Check.h"
#include "CorridorApronBlend.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <new>

namespace {
std::atomic<size_t> allocations{0};
}

void *operator new(size_t bytes) {
  void *block = std::malloc(bytes == 0 ? 1 : bytes);
  if (block == nullptr) { std::abort(); }
  allocations.fetch_add(1, std::memory_order_relaxed);
  return block;
}

void operator delete(void *block) noexcept {
  std::free(block);
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::array<EarthworkStamp, 5> stamps;
  for (auto &stamp : stamps) {
    stamp.Kind = EarthworkKind::Corridor;
    stamp.CorridorKey = 42;
    stamp.Fills = true;
  }
  stamps[2].Fills = false;
  stamps[3].Profile = ProfiledCorridorSpan{};
  stamps[4].CorridorKey = 0;
  CorridorApronBlend blend(stamps);
  CHECK(!blend.Add(3, {.Weight = 1, .CorrectionM = 2}) &&
            !blend.Add(4, {.Weight = 1, .CorrectionM = 2}),
        "profiled and unrelated contacts retain their own influence path");
  CHECK(blend.Add(0, {.Weight = 0, .CorrectionM = 9}) && blend.Fold().Weight == 0,
        "zero influence cannot create an active group or a division by zero");
  const size_t bytes = blend.HeapBytes();
  const auto before = allocations.load();
  bool accepted = true;
  double weight = 0;
  double correction = 0;
  for (int point = 0; point < 10000; ++point) {
    blend.Reset();
    accepted = blend.Add(0, {.Weight = 2, .CorrectionM = 1}) &&
               blend.Add(1, {.Weight = 3, .CorrectionM = 3}) &&
               blend.Add(2, {.Weight = 4, .CorrectionM = -2}) && accepted;
    const auto result = blend.Fold();
    weight = result.Weight;
    correction = result.CorrectionM;
  }
  const auto after = allocations.load();
  CHECK(accepted && weight == 7 && std::abs(correction + 1.4) < 1.e-12,
        "one envelope per contact family preserves a weighted target without mixing fill modes");
  CHECK(before == after && blend.HeapBytes() == bytes,
        "point collection reuses bounded group storage without allocating");
  blend.Reset();
  CHECK(blend.Fold().Weight == 0 && blend.Fold().CorrectionM == 0,
        "a new point cannot inherit a preceding point's terrain correction");
  return Report();
}
