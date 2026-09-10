#include "Heap.h"

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <algorithm>
#include <mutex>
#include <string_view>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include "HeapProbe.h"

namespace outshine {

constexpr size_t kCountDigits = 24;

namespace {

std::atomic<size_t> gLiveBytes{0};
std::atomic<bool> gProcessInstrumentation{false};

constexpr size_t kTagSlots = 32;
constexpr size_t kTagNameBytes = 96;
constexpr size_t kOverflowTag = 0;
constexpr size_t kUntaggedTag = 1;

thread_local size_t gTagIndex = kUntaggedTag;

struct TagRow {
  std::array<char, kTagNameBytes> Name{};
  std::atomic<size_t> Taken{0};
  std::atomic<bool> Published;

  constexpr TagRow() noexcept : Published(false) {}

  template <size_t N>
  constexpr explicit TagRow(const std::array<char, N> &name) noexcept : Published(N > 1) {
    static_assert(N <= kTagNameBytes);
    std::ranges::copy(name, Name.begin());
  }
};

constinit std::array<TagRow, kTagSlots> gTags{TagRow{std::to_array("other")},
                                              TagRow{std::to_array("untagged")}};
std::mutex gTagMutex;

size_t TagIndex(const char *tag) {
  if (tag == nullptr || *tag == '\0') { return kUntaggedTag; }
  size_t length = 0;
  while (length < kTagNameBytes && tag[length] != '\0') { ++length; }
  if (length == kTagNameBytes) { return kOverflowTag; }
  const std::string_view name(tag, length);
  const std::scoped_lock lock(gTagMutex);
  for (size_t at = 0; at < kTagSlots; ++at) {
    TagRow &row = gTags[at];
    if (row.Published.load(std::memory_order_relaxed)) {
      if (std::string_view(row.Name.data()) == name) { return at; }
      continue;
    }
    std::ranges::copy(name, row.Name.begin());
    row.Published.store(true, std::memory_order_release);
    return at;
  }
  return kOverflowTag;
}

inline size_t BlockBytes(const void *block) {
#ifdef __APPLE__
  return malloc_size(block);
#else
  return malloc_usable_size(block);
#endif
}

inline void *Counted(void *block) {
  if (block == nullptr) { return block; }
  const size_t bytes = BlockBytes(block);
  gLiveBytes.fetch_add(bytes, std::memory_order_relaxed);
  gTags[gTagIndex].Taken.fetch_add(bytes, std::memory_order_relaxed);
  return block;
}

inline void Returned(void *block) noexcept {
  if (block == nullptr) { return; }
  gLiveBytes.fetch_sub(BlockBytes(block), std::memory_order_relaxed);
  std::free(block);
}

[[noreturn]] void End(const char *item, const char *bytes) {
  std::array<char, 256> line{};
  std::snprintf(line.data(),
                line.size(),
                "outshine heap exhausted: item=%s bytes=%s liveBytes=%zu breakBytes=%zu\n",
                item,
                bytes,
                HeapProbe::LiveBytes(),
                HeapProbe::BreakBytes());
  std::fputs(line.data(), stderr);
  std::fflush(stderr);
  std::abort();
}

[[noreturn]] void EndWithCount(const char *item, size_t bytes) {
  std::array<char, kCountDigits> count{};
  std::snprintf(count.data(), count.size(), "%zu", bytes);
  End(item, count.data());
}

}

void *Heap::Take(const char *item, size_t bytes) {
  void *block = std::malloc((bytes != 0u) ? bytes : 1);
  if (block == nullptr) { EndWithCount(item, bytes); }
  return Counted(block);
}

void Heap::EnableProcessInstrumentation() noexcept {
  gProcessInstrumentation.store(true, std::memory_order_relaxed);
}

bool Heap::ProcessInstrumentationEnabled() noexcept {
  return gProcessInstrumentation.load(std::memory_order_relaxed);
}

void *Heap::TryTake(size_t bytes) noexcept {
  return Counted(std::malloc(bytes != 0 ? bytes : 1));
}

void *Heap::TryTakeAligned(size_t bytes, size_t alignment) noexcept {
  void *block = nullptr;
  if (posix_memalign(&block,
                     alignment < sizeof(void *) ? sizeof(void *) : alignment,
                     bytes != 0 ? bytes : 1) != 0) {
    return nullptr;
  }
  return Counted(block);
}

void *Heap::TakeAligned(const char *item, size_t bytes, size_t alignment) {
  void *block = TryTakeAligned(bytes, alignment);
  if (block == nullptr) { EndWithCount(item, bytes); }
  return block;
}

void Heap::Return(void *block) noexcept {
  Returned(block);
}

size_t Heap::LiveBytes() {
  return gLiveBytes.load(std::memory_order_relaxed);
}

Heap::Tagged::Tagged(const char *tag) noexcept : Held_(gTagIndex) {
  gTagIndex = TagIndex(tag);
}

Heap::Tagged::~Tagged() noexcept {
  gTagIndex = Held_;
}

size_t Heap::TakenUnder(const char *tag) {
  return gTags[TagIndex(tag)].Taken.load(std::memory_order_relaxed);
}

size_t Heap::TagCount() {
  return kTagSlots;
}

const char *Heap::TagAt(size_t at) {
  return at < kTagSlots && gTags[at].Published.load(std::memory_order_acquire)
             ? gTags[at].Name.data()
             : nullptr;
}

size_t Heap::TakenAt(size_t at) {
  return at < kTagSlots ? gTags[at].Taken.load(std::memory_order_relaxed) : 0;
}

void Heap::Exhausted(const char *item) {
  End(item, "unstated");
}

}
