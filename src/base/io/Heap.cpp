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

std::atomic<size_t> gLiveBytes{0};

constexpr size_t kTagSlots = 32;
constexpr const char *kUntagged = "untagged";
constexpr const char *kOverflow = "other";

thread_local const char *gTag = nullptr;

struct TagRow {
  std::atomic<const char *> Name{nullptr};
  std::atomic<size_t> Taken{0};
};

TagRow gTags[kTagSlots];

TagRow *RowFor(const char *tag) {
  if (gTags[0].Name.load(std::memory_order_relaxed) == nullptr) {
    const char *empty = nullptr;
    gTags[0].Name.compare_exchange_strong(empty, kOverflow, std::memory_order_relaxed);
  }
  if (tag == kOverflow) { return &gTags[0]; }
  for (size_t at = 1; at < kTagSlots; ++at) {
    const char *held = gTags[at].Name.load(std::memory_order_relaxed);
    if (held == tag) { return &gTags[at]; }
    if (held == nullptr) {
      const char *empty = nullptr;
      if (gTags[at].Name.compare_exchange_strong(empty, tag, std::memory_order_relaxed)) {
        return &gTags[at];
      }
      if (gTags[at].Name.load(std::memory_order_relaxed) == tag) { return &gTags[at]; }
    }
  }
  return nullptr;
}

inline size_t BlockBytes(void *block) {
#if defined(__APPLE__)
  return malloc_size(block);
#else
  return malloc_usable_size(block);
#endif
}

inline void *Counted(void *block) {
  if (block == nullptr) { return block; }
  const size_t bytes = BlockBytes(block);
  gLiveBytes.fetch_add(bytes, std::memory_order_relaxed);
  TagRow *row = RowFor(gTag != nullptr ? gTag : kUntagged);
  if (row == nullptr) { row = RowFor(kOverflow); }
  row->Taken.fetch_add(bytes, std::memory_order_relaxed);
  return block;
}

inline void Returned(void *block) noexcept {
  if (block == nullptr) { return; }
  gLiveBytes.fetch_sub(BlockBytes(block), std::memory_order_relaxed);
  std::free(block);
}

[[noreturn]] void End(const char *item, const char *bytes) {
  char line[256];
  std::snprintf(line,
                sizeof line,
                "outshine heap exhausted: item=%s bytes=%s liveBytes=%zu breakBytes=%zu\n",
                item,
                bytes,
                HeapProbe::LiveBytes(),
                HeapProbe::BreakBytes());
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
  if (posix_memalign(&block,
                     alignment < sizeof(void *) ? sizeof(void *) : alignment,
                     bytes ? bytes : 1) != 0) {
    EndWithCount(item, bytes);
  }
  return Counted(block);
}

} // namespace

void *Heap::Take(const char *item, size_t bytes) {
  void *block = std::malloc(bytes ? bytes : 1);
  if (!block) { EndWithCount(item, bytes); }
  return Counted(block);
}

size_t Heap::LiveBytes() {
  return gLiveBytes.load(std::memory_order_relaxed);
}

Heap::Tagged::Tagged(const char *tag) noexcept : Held_(gTag) {
  gTag = tag;
}

Heap::Tagged::~Tagged() noexcept {
  gTag = Held_;
}

size_t Heap::TakenUnder(const char *tag) {
  const TagRow *row = RowFor(tag);
  return row != nullptr ? row->Taken.load(std::memory_order_relaxed) : 0;
}

size_t Heap::TagCount() {
  return kTagSlots;
}

const char *Heap::TagAt(size_t at) {
  return at < kTagSlots ? gTags[at].Name.load(std::memory_order_relaxed) : nullptr;
}

size_t Heap::TakenAt(size_t at) {
  return at < kTagSlots ? gTags[at].Taken.load(std::memory_order_relaxed) : 0;
}

void Heap::Exhausted(const char *item) {
  End(item, "unstated");
}

} // namespace outshine

void *operator new(size_t bytes) {
  return outshine::Heap::Take("object", bytes);
}

void *operator new[](size_t bytes) {
  return outshine::Heap::Take("object array", bytes);
}

void *operator new(size_t bytes, std::align_val_t alignment) {
  return outshine::TakeAligned("object", bytes, (size_t)alignment);
}

void *operator new[](size_t bytes, std::align_val_t alignment) {
  return outshine::TakeAligned("object array", bytes, (size_t)alignment);
}

void *operator new(size_t bytes, const std::nothrow_t &) noexcept {
  return std::malloc(bytes ? bytes : 1);
}

void *operator new[](size_t bytes, const std::nothrow_t &) noexcept {
  return outshine::Counted(std::malloc(bytes ? bytes : 1));
}

void operator delete(void *block) noexcept {
  outshine::Returned(block);
}

void operator delete[](void *block) noexcept {
  outshine::Returned(block);
}

void operator delete(void *block, size_t) noexcept {
  outshine::Returned(block);
}

void operator delete[](void *block, size_t) noexcept {
  outshine::Returned(block);
}

void operator delete(void *block, std::align_val_t) noexcept {
  outshine::Returned(block);
}

void operator delete[](void *block, std::align_val_t) noexcept {
  outshine::Returned(block);
}

void operator delete(void *block, size_t, std::align_val_t) noexcept {
  outshine::Returned(block);
}

void operator delete[](void *block, size_t, std::align_val_t) noexcept {
  outshine::Returned(block);
}

void operator delete(void *block, const std::nothrow_t &) noexcept {
  outshine::Returned(block);
}

void operator delete[](void *block, const std::nothrow_t &) noexcept {
  outshine::Returned(block);
}
