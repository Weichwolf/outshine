#include "HeapProbe.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ratio>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

namespace outshine {

namespace {

std::atomic<size_t> gPeakLive{0};
std::atomic<double> gCostMs{0.0};

} // namespace

size_t HeapProbe::LiveBytes() {
#ifdef __APPLE__
  malloc_statistics_t s{};
  malloc_zone_statistics(malloc_default_zone(), &s);
  return s.size_in_use;
#else
  const struct mallinfo2 m = mallinfo2();
  return (size_t)m.uordblks;
#endif
}

size_t HeapProbe::BreakBytes() {
#ifdef __APPLE__

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
  gCostMs.store(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(),
      std::memory_order_relaxed);
  size_t seen = gPeakLive.load(std::memory_order_relaxed);
  while (now > seen && !gPeakLive.compare_exchange_weak(seen, now, std::memory_order_relaxed)) {}
  return now;
}

void HeapProbe::ForgetPeak() {
  gPeakLive.store(0, std::memory_order_relaxed);
}

size_t HeapProbe::PeakLiveBytes() {
  return gPeakLive.load(std::memory_order_relaxed);
}

double HeapProbe::SampleCostMs() {
  return gCostMs.load(std::memory_order_relaxed);
}

} // namespace outshine
