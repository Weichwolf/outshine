#include "HeapProbe.h"

#include <atomic>
#include <cstdint>

#ifdef __EMSCRIPTEN__
#include <emscripten/heap.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

namespace outshine {

namespace {

std::atomic<size_t> gPeak{0};

}  // namespace

size_t HeapProbe::Bytes() {
#ifdef __EMSCRIPTEN__
  return (size_t)(uintptr_t)sbrk(0);
#elif defined(__APPLE__)
  malloc_statistics_t s{};
  malloc_zone_statistics(malloc_default_zone(), &s);
  return s.size_in_use;
#else
  return (size_t)mallinfo2().uordblks;
#endif
}

void HeapProbe::Sample() {
  const size_t now = Bytes();
  size_t seen = gPeak.load(std::memory_order_relaxed);
  while (now > seen && !gPeak.compare_exchange_weak(seen, now, std::memory_order_relaxed)) {
  }
}

size_t HeapProbe::PeakBytes() { return gPeak.load(std::memory_order_relaxed); }

size_t HeapProbe::CeilingBytes() {
#ifdef __EMSCRIPTEN__
  return emscripten_get_heap_max();
#else
  return 0;
#endif
}

} // namespace outshine
