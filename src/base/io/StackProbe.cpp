#include "StackProbe.h"

#include <cstdint>

#include <pthread.h>

namespace outshine {

namespace {

constexpr uint64_t kPaint = 0xA5C35A3CA5C35A3Cull;

constexpr size_t kLiveMargin = 4096;

constexpr size_t kPaintSpan = 512u * 1024u;

constexpr size_t kToolchainCookie = 16;

struct Slot {
  std::atomic<size_t> Peak{0};
  std::atomic<size_t> Capacity{0};
  std::atomic<size_t> Floor{0};
  std::atomic<size_t> Limit{0};
};

Slot &SlotOf(StackProbe::Purpose p) {
  static Slot slots[StackProbe::kPurposeCount];
  return slots[(int)p];
}

thread_local uintptr_t tBase = 0, tPaintBottom = 0, tPaintTop = 0;
thread_local StackProbe::Purpose tPurpose = StackProbe::Purpose::Frame;

void ThisStack(uintptr_t *base, uintptr_t *end, uintptr_t *current) {
#if defined(__APPLE__)
  const pthread_t self = pthread_self();
  *base = (uintptr_t)pthread_get_stackaddr_np(self);
  *end = *base - (uintptr_t)pthread_get_stacksize_np(self);
  *current = (uintptr_t)__builtin_frame_address(0);
#else
  pthread_attr_t attr;
  void *low = nullptr;
  size_t size = 0;
  pthread_getattr_np(pthread_self(), &attr);
  pthread_attr_getstack(&attr, &low, &size);
  pthread_attr_destroy(&attr);
  *end = (uintptr_t)low;
  *base = *end + size;
  *current = (uintptr_t)__builtin_frame_address(0);
#endif
}

void Raise(std::atomic<size_t> &target, size_t value) {
  size_t seen = target.load(std::memory_order_relaxed);
  while (value > seen && !target.compare_exchange_weak(seen, value, std::memory_order_relaxed)) {}
}

} // namespace

void StackProbe::Enter(Purpose purpose) {
  uintptr_t base = 0, end = 0, current = 0;
  ThisStack(&base, &end, &current);
  const uintptr_t deepest = end + kToolchainCookie;
  if (base == 0 || current < deepest + kLiveMargin + sizeof(uint64_t)) { return; }
  const uintptr_t top = (current - kLiveMargin) & ~(uintptr_t)7;

  const uintptr_t bottom = top >= deepest + kPaintSpan ? top - kPaintSpan : deepest;
  for (uintptr_t a = bottom; a < top; a += sizeof(uint64_t)) { *(uint64_t *)a = kPaint; }
  tBase = base;
  tPaintBottom = bottom;
  tPaintTop = top;
  tPurpose = purpose;
  Slot &slot = SlotOf(purpose);
  Raise(slot.Capacity, (size_t)(base - end));
  Raise(slot.Floor, (size_t)(base - top));
  Raise(slot.Limit, (size_t)(base - bottom));
  Mark();
}

void StackProbe::Mark() {
  if (tBase == 0) { return; }
  uintptr_t frontier = tPaintTop;
  for (uintptr_t a = tPaintBottom; a < tPaintTop; a += sizeof(uint64_t)) {
    if (*(const uint64_t *)a != kPaint) {
      frontier = a;
      break;
    }
  }
  Raise(SlotOf(tPurpose).Peak, (size_t)(tBase - frontier));
}

size_t StackProbe::PeakBytes(Purpose purpose) {
  return SlotOf(purpose).Peak.load(std::memory_order_relaxed);
}

size_t StackProbe::CapacityBytes(Purpose purpose) {
  return SlotOf(purpose).Capacity.load(std::memory_order_relaxed);
}

size_t StackProbe::FloorBytes(Purpose purpose) {
  return SlotOf(purpose).Floor.load(std::memory_order_relaxed);
}

size_t StackProbe::LimitBytes(Purpose purpose) {
  return SlotOf(purpose).Limit.load(std::memory_order_relaxed);
}

const char *StackProbe::Name(Purpose purpose) {
  switch (purpose) {
    case Purpose::Frame: return "frame";
    case Purpose::Class: return "class";
    case Purpose::Tile: return "tile";
    case Purpose::Region: return "region";
  }
  return "";
}

} // namespace outshine
