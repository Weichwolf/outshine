#include <new>
#include "../../../src/base/io/Heap.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (size_t bytes : {size_t{0}, size_t{1}, size_t{257}}) {
    const size_t before = Heap::LiveBytes();
    void *block = ::operator new(bytes, std::nothrow);
    const bool allocated = block != nullptr;
    const size_t during = Heap::LiveBytes();
    ::operator delete(block);
    const size_t after = Heap::LiveBytes();
    CHECK(allocated, "small nothrow scalar allocation succeeds");
    CHECK(during > before && during - before >= bytes, "scalar allocation is counted");
    CHECK(after == before, "scalar release restores live-byte count");
    const size_t arrayBefore = Heap::LiveBytes();
    block = ::operator new[](bytes, std::nothrow);
    const bool arrayAllocated = block != nullptr;
    const size_t arrayDuring = Heap::LiveBytes();
    ::operator delete[](block);
    const size_t arrayAfter = Heap::LiveBytes();
    CHECK(arrayAllocated, "small nothrow array allocation succeeds");
    CHECK(arrayDuring > arrayBefore && arrayDuring - arrayBefore >= bytes,
          "array allocation is counted");
    CHECK(arrayAfter == arrayBefore, "array release restores live-byte count");
  }
  const size_t beforeNull = Heap::LiveBytes();
  ::operator delete(nullptr);
  ::operator delete[](nullptr);
  CHECK(Heap::LiveBytes() == beforeNull, "null release leaves counters unchanged");
  return Report();
}
