#include <cstdint>
#include <limits>
#include <new>
#include "../../../src/base/io/Heap.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (size_t alignment : {size_t{64}, size_t{256}, size_t{4096}}) {
    const auto aligned = static_cast<std::align_val_t>(alignment);
    for (size_t bytes : {size_t{0}, size_t{1}, size_t{257}}) {
      for (bool array : {false, true}) {
        const size_t before = Heap::LiveBytes();
        void *block = array ? ::operator new[](bytes, aligned, std::nothrow)
                            : ::operator new(bytes, aligned, std::nothrow);
        const bool allocated = block != nullptr;
        const bool positioned = reinterpret_cast<uintptr_t>(block) % alignment == 0;
        const size_t during = Heap::LiveBytes();
        if (array) {
          ::operator delete[](block, aligned, std::nothrow);
        } else {
          ::operator delete(block, aligned, std::nothrow);
        }
        const size_t after = Heap::LiveBytes();
        CHECK(allocated && positioned, "nothrow allocation satisfies requested alignment");
        CHECK(during > before && during - before >= bytes, "aligned allocation is counted");
        CHECK(after == before, "matching aligned cleanup restores live-byte count");
      }
    }
  }
  // SIZE_MAX cannot fit in the process address space; no actual memory pressure is required.
  volatile size_t impossible = std::numeric_limits<size_t>::max();
  for (bool array : {false, true}) {
    const size_t before = Heap::LiveBytes();
    constexpr auto alignment = std::align_val_t{64};
    void *block = array ? ::operator new[](impossible, alignment, std::nothrow)
                        : ::operator new(impossible, alignment, std::nothrow);
    const bool refused = block == nullptr;
    if (array) {
      ::operator delete[](block, alignment, std::nothrow);
    } else {
      ::operator delete(block, alignment, std::nothrow);
    }
    const size_t after = Heap::LiveBytes();
    CHECK(refused, "unrepresentable allocation returns null without terminating");
    CHECK(after == before, "failed allocation and null cleanup preserve accounting");
  }
  return Report();
}
