#include "Heap.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include "HeapProbe.h"

namespace outshine {

namespace {

/* THE ONE COUNTER, AND IT IS A NAMESPACE-SCOPE ATOMIC SO IT IS ZERO BEFORE ANY DYNAMIC INITIALISATION
 * RUNS. An `operator new` that read a counter constructed later would count the program's own startup
 * against a lifetime that had not begun. */
std::atomic<size_t> gLiveBytes{0};

/* WHAT THE ALLOCATOR ACTUALLY SET ASIDE FOR THIS BLOCK, asked of the allocator rather than remembered.
 * A table from pointer to size would be an allocation on the free path, which is the shape this whole
 * item exists to remove. */
inline size_t BlockBytes(void *block) {
#if defined(__APPLE__)
  return malloc_size(block);
#else
  return malloc_usable_size(block);
#endif
}

inline void *Counted(void *block) {
  if (block != nullptr) { gLiveBytes.fetch_add(BlockBytes(block), std::memory_order_relaxed); }
  return block;
}

/* THE RETURN IS SUBTRACTED BEFORE THE BLOCK IS FREED, because after `free` the size is not askable. */
inline void Returned(void *block) noexcept {
  if (block == nullptr) { return; }
  gLiveBytes.fetch_sub(BlockBytes(block), std::memory_order_relaxed);
  std::free(block);
}

[[noreturn]] void End(const char *item, const char *bytes) {
  /* Log and Telemetry both allocate, and what just failed is the allocator they would allocate from:
   * this one line goes out on the rawest sink the platform has. */
  char line[256];
  std::snprintf(line, sizeof line,
                "outshine heap exhausted: item=%s bytes=%s liveBytes=%zu breakBytes=%zu\n",
                item, bytes, HeapProbe::LiveBytes(), HeapProbe::BreakBytes());
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
  return Counted(block);
}

} // namespace

void *Heap::Take(const char *item, size_t bytes) {
  void *block = std::malloc(bytes ? bytes : 1);
  if (!block) EndWithCount(item, bytes);
  return Counted(block);
}

size_t Heap::LiveBytes() { return gLiveBytes.load(std::memory_order_relaxed); }

void Heap::Exhausted(const char *item) { End(item, "unstated"); }

} // namespace outshine

/* THE SAME POLICY FOR EVERY OBJECT IN THE PROGRAM, and it lives here because it is one decision and
 * not two.
 *
 * **BOTH SIDES ARE REPLACED NOW (board:1462), and they have to be.** Deallocation used to be left to
 * the standard library, which is correct and answers nothing: a count of what this engine holds needs
 * the returning half as much as the taking one. Every deallocation form the language can call is here,
 * because a form left out would subtract nothing and the counter would only ever rise. */
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
  return outshine::Counted(std::malloc(bytes ? bytes : 1));
}

/* THE RETURNING HALF. `free` releases what `malloc` and `posix_memalign` hand out on every platform
 * this engine targets, so one implementation serves the aligned and unaligned forms alike -- and the
 * SIZED forms are given their own bodies rather than a default, because a default would let the
 * compiler pick the unsized one and the two would count differently. */
void operator delete(void *block) noexcept { outshine::Returned(block); }
void operator delete[](void *block) noexcept { outshine::Returned(block); }
void operator delete(void *block, size_t) noexcept { outshine::Returned(block); }
void operator delete[](void *block, size_t) noexcept { outshine::Returned(block); }
void operator delete(void *block, std::align_val_t) noexcept { outshine::Returned(block); }
void operator delete[](void *block, std::align_val_t) noexcept { outshine::Returned(block); }
void operator delete(void *block, size_t, std::align_val_t) noexcept { outshine::Returned(block); }
void operator delete[](void *block, size_t, std::align_val_t) noexcept { outshine::Returned(block); }
void operator delete(void *block, const std::nothrow_t &) noexcept { outshine::Returned(block); }
void operator delete[](void *block, const std::nothrow_t &) noexcept { outshine::Returned(block); }
