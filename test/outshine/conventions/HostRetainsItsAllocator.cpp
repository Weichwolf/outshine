#include <atomic>
#include <cstdlib>
#include <new>
#include "../../../src/base/io/Heap.h"
#include "Check.h"

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
  CHECK(!Heap::ProcessInstrumentationEnabled(), "library does not install process instrumentation");
  const size_t engineBefore = Heap::LiveBytes();
  const size_t hostBefore = allocations.load();
  void *block = ::operator new(32);
  const size_t hostAfter = allocations.load();
  ::operator delete(block);
  CHECK(hostAfter == hostBefore + 1, "host allocator remains installed");
  CHECK(Heap::LiveBytes() == engineBefore, "host allocation is not charged to engine counters");
  return Report();
}
