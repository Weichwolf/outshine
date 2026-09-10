#include "Heap.h"
#include "Check.h"
#include <cstdint>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (size_t alignment : {size_t{0},
                           size_t{3},
                           size_t{5},
                           size_t{7},
                           size_t{24},
                           std::numeric_limits<size_t>::max()}) {
    const size_t before = Heap::LiveBytes();
    void *block = Heap::TryTakeAligned(32, alignment);
    const bool refused = block == nullptr;
    Heap::Return(block);
    CHECK(refused, "invalid alignment refused before allocation");
    CHECK(Heap::LiveBytes() == before, "refusal preserves byte count");
  }
  for (size_t alignment : {size_t{1}, size_t{2}, size_t{4}, size_t{8}, size_t{64}, size_t{4096}}) {
    for (size_t bytes : {size_t{0}, size_t{17}}) {
      const size_t before = Heap::LiveBytes();
      void *block = Heap::TryTakeAligned(bytes, alignment);
      const bool aligned = block != nullptr && reinterpret_cast<uintptr_t>(block) % alignment == 0;
      const size_t during = Heap::LiveBytes();
      Heap::Return(block);
      CHECK(aligned, "valid alignment satisfied including zero-size request");
      CHECK(during > before && during - before >= bytes && Heap::LiveBytes() == before,
            "valid allocation and release have symmetric counters");
    }
  }
  return Report();
}
