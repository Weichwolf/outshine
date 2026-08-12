#include "HeapProbe.h"

#include <atomic>
#include <chrono>
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

std::atomic<size_t> gPeakLive{0};
std::atomic<double> gCostMs{0.0};

#ifdef __EMSCRIPTEN__

/* dlmalloc's statistics struct, which emscripten's <malloc.h> declares nowhere: ten fields of
 * MALLINFO_FIELD_TYPE, which is size_t in the dlmalloc this sysroot links. Believing that is what
 * LayoutHolds() below removes — internal_mallinfo() fills all four of these from one walk, so
 * in-use plus free is everything the segments hold, and a field order that had slipped could not
 * satisfy it. */
struct DlMallinfo {
  size_t Arena, OrdBlks, SmBlks, HBlks, HBlkHd, UsmBlks, FsmBlks, UordBlks, FordBlks, KeepCost;
};
extern "C" DlMallinfo mallinfo(void);

[[nodiscard]] bool LayoutHolds() {
  const DlMallinfo m = mallinfo();
  return m.UordBlks + m.FordBlks == m.Arena + m.HBlkHd;
}

const bool gLayoutHolds = LayoutHolds();

#endif

}  // namespace

bool HeapProbe::LiveBytesKnown() {
#ifdef __EMSCRIPTEN__
  return gLayoutHolds;
#else
  return true;
#endif
}

size_t HeapProbe::LiveBytes() {
#ifdef __EMSCRIPTEN__
  return gLayoutHolds ? mallinfo().UordBlks : 0;
#elif defined(__APPLE__)
  malloc_statistics_t s{};
  malloc_zone_statistics(malloc_default_zone(), &s);
  return s.size_in_use;
#else
  const struct mallinfo2 m = mallinfo2();
  return (size_t)m.uordblks;
#endif
}

size_t HeapProbe::BreakBytes() {
#ifdef __EMSCRIPTEN__
  return (size_t)(uintptr_t)sbrk(0);
#elif defined(__APPLE__)
  /* The default zone's own footprint. Not a break — macOS has no single one — but the same
   * question: what this allocator took from the system rather than what it is handing out. */
  malloc_statistics_t s{};
  malloc_zone_statistics(malloc_default_zone(), &s);
  return s.size_allocated;
#else
  const struct mallinfo2 m = mallinfo2();
  return (size_t)(m.arena + m.hblkhd);
#endif
}

size_t HeapProbe::Sample() {
  const auto t0 = std::chrono::steady_clock::now();
  const size_t now = LiveBytes();
  gCostMs.store(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(),
                std::memory_order_relaxed);
  size_t seen = gPeakLive.load(std::memory_order_relaxed);
  while (now > seen && !gPeakLive.compare_exchange_weak(seen, now, std::memory_order_relaxed)) {
  }
  return now;
}

size_t HeapProbe::PeakLiveBytes() { return gPeakLive.load(std::memory_order_relaxed); }

double HeapProbe::SampleCostMs() { return gCostMs.load(std::memory_order_relaxed); }

size_t HeapProbe::ReservedBytes() {
#ifdef __EMSCRIPTEN__
  return emscripten_get_heap_size();
#else
  return 0;
#endif
}

} // namespace outshine
