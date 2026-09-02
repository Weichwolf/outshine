#include "StackProbe.h"

#include <array>
#include <cstddef>
#include <atomic>
#include <cstdint>

#include <pthread.h>

namespace outshine {

namespace {

constexpr uint64_t kPaint = 0xA5C35A3CA5C35A3Cull;

constexpr size_t kLiveMargin = 4096;

constexpr size_t kPaintSpan = size_t{512} * 1024;

constexpr size_t kToolchainCookie = 16;

struct Slot {
  std::atomic<size_t> Peak{0};
  std::atomic<size_t> Capacity{0};
  std::atomic<size_t> Floor{0};
  std::atomic<size_t> Limit{0};
};

Slot &SlotOf(StackProbe::Purpose p) {
  static std::array<Slot, StackProbe::kPurposeCount> slots{};
  return slots[static_cast<int>(p)];
}

thread_local std::byte *tBase = nullptr, *tPaintBottom = nullptr, *tPaintTop = nullptr;
thread_local StackProbe::Purpose tPurpose = StackProbe::Purpose::Frame;

void ThisStack(std::byte **base, std::byte **end, std::byte **current) {
#ifdef __APPLE__
  const pthread_t self = pthread_self();
  *base = static_cast<std::byte *>(pthread_get_stackaddr_np(self));
  *end = *base - pthread_get_stacksize_np(self);
  *current = static_cast<std::byte *>(__builtin_frame_address(0));
#else
  pthread_attr_t attr;
  void *low = nullptr;
  size_t size = 0;
  pthread_getattr_np(pthread_self(), &attr);
  pthread_attr_getstack(&attr, &low, &size);
  pthread_attr_destroy(&attr);
  *end = static_cast<std::byte *>(low);
  *base = *end + size;
  *current = static_cast<std::byte *>(__builtin_frame_address(0));
#endif
}

void Raise(std::atomic<size_t> &target, size_t value) {
  size_t seen = target.load(std::memory_order_relaxed);
  while (value > seen && !target.compare_exchange_weak(seen, value, std::memory_order_relaxed)) {}
}

} // namespace

void StackProbe::Enter(Purpose purpose) {
  std::byte *base = nullptr;
  std::byte *end = nullptr;
  std::byte *current = nullptr;
  ThisStack(&base, &end, &current);
  std::byte *const deepest = end + kToolchainCookie;
  if (base == nullptr || current < deepest + kLiveMargin + sizeof(uint64_t)) { return; }
  std::byte *top = current - kLiveMargin;
  top -= reinterpret_cast<uintptr_t>(top) & static_cast<uintptr_t>(7);

  std::byte *const bottom = top >= deepest + kPaintSpan ? top - kPaintSpan : deepest;
  for (std::byte *at = bottom; at < top; at += sizeof(uint64_t)) {
    *reinterpret_cast<uint64_t *>(at) = kPaint;
  }
  tBase = base;
  tPaintBottom = bottom;
  tPaintTop = top;
  tPurpose = purpose;
  Slot &slot = SlotOf(purpose);
  Raise(slot.Capacity, static_cast<size_t>(base - end));
  Raise(slot.Floor, static_cast<size_t>(base - top));
  Raise(slot.Limit, static_cast<size_t>(base - bottom));
  Mark();
}

void StackProbe::Mark() {
  if (tBase == nullptr) { return; }
  const std::byte *frontier = tPaintTop;
  for (const std::byte *at = tPaintBottom; at < tPaintTop; at += sizeof(uint64_t)) {
    if (*reinterpret_cast<const uint64_t *>(at) != kPaint) {
      frontier = at;
      break;
    }
  }
  Raise(SlotOf(tPurpose).Peak, static_cast<size_t>(tBase - frontier));
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
