#include "Heap.h"

#include <cstdio>
#include <cstdlib>
#include <new>

#include "HeapProbe.h"

namespace outshine {

namespace {

[[noreturn]] void End(const char *item, const char *bytes) {
  /* Log and Telemetry both allocate, and what just failed is the allocator they would allocate from:
   * this one line goes out on the rawest sink the platform has. */
  char line[256];
  std::snprintf(line, sizeof line,
                "outshine heap exhausted: item=%s bytes=%s liveBytes=%zu breakBytes=%zu "
                "reservedBytes=%zu\n",
                item, bytes, HeapProbe::LiveBytes(), HeapProbe::BreakBytes(),
                HeapProbe::ReservedBytes());
  std::fputs(line, stderr);
  std::fflush(stderr);
  std::abort();
}

[[noreturn]] void EndWithCount(const char *item, size_t bytes) {
  char count[24];
  std::snprintf(count, sizeof count, "%zu", bytes);
  End(item, count);
}

void *TakeAligned(const char *item, size_t bytes, size_t alignment) {
  void *block = nullptr;
  if (posix_memalign(&block, alignment < sizeof(void *) ? sizeof(void *) : alignment,
                     bytes ? bytes : 1) != 0)
    EndWithCount(item, bytes);
  return block;
}

} // namespace

void *Heap::Take(const char *item, size_t bytes) {
  void *block = std::malloc(bytes ? bytes : 1);
  if (!block) EndWithCount(item, bytes);
  return block;
}

void Heap::Exhausted(const char *item) { End(item, "unstated"); }

} // namespace outshine

/* THE SAME POLICY FOR EVERY OBJECT IN THE PROGRAM, and it lives here because it is one decision and
 * not two. The replaced functions are the allocating ones only: deallocation is left to the standard
 * library, which frees exactly what malloc and posix_memalign hand out. */
void *operator new(size_t bytes) { return outshine::Heap::Take("object", bytes); }
void *operator new[](size_t bytes) { return outshine::Heap::Take("object array", bytes); }
void *operator new(size_t bytes, std::align_val_t alignment) {
  return outshine::TakeAligned("object", bytes, (size_t)alignment);
}
void *operator new[](size_t bytes, std::align_val_t alignment) {
  return outshine::TakeAligned("object array", bytes, (size_t)alignment);
}
/* nothrow means the caller has its own answer to failure, so it gets the null it asked for. */
void *operator new(size_t bytes, const std::nothrow_t &) noexcept {
  return std::malloc(bytes ? bytes : 1);
}
void *operator new[](size_t bytes, const std::nothrow_t &) noexcept {
  return std::malloc(bytes ? bytes : 1);
}
