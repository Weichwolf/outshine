#include "StackProbe.h"

#include <cstdint>

#ifdef __EMSCRIPTEN__
#include <emscripten/stack.h>
#else
#include <pthread.h>
#endif

namespace outshine {

namespace {

/* A word that no plausible pointer, float or ASCII run reproduces. */
constexpr uint64_t kPaint = 0xA5C35A3CA5C35A3Cull;
/* Below the caller's own frame: the paint must not reach the frame that is painting, nor the frames
 * the paint itself pushes. */
constexpr size_t kLiveMargin = 4096;
/* [SET] How far below the entry point the probe can see. The scan is linear and exact over exactly
 * this much, so the span is the price of a Mark: 64 Ki word compares. Against a deepest observed 204
 * KiB it leaves a factor of two and a half, and a thread that outruns it says so — Peak reaches
 * Limit rather than passing it. */
constexpr size_t kPaintSpan = 512u * 1024u;
/* The toolchain writes its own stack-overflow cookie at the deepest address of the stack and aborts
 * when it no longer reads back — so the paint stops short of it and the two instruments coexist:
 * this one says how deep the thread went, that one says when it went past the end. */
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
#ifdef __EMSCRIPTEN__
  *base = emscripten_stack_get_base();
  *end = emscripten_stack_get_end();
  *current = emscripten_stack_get_current();
#elif defined(__APPLE__)
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
  while (value > seen && !target.compare_exchange_weak(seen, value, std::memory_order_relaxed)) {
  }
}

} // namespace

void StackProbe::Enter(Purpose purpose) {
  uintptr_t base = 0, end = 0, current = 0;
  ThisStack(&base, &end, &current);
  const uintptr_t deepest = end + kToolchainCookie;
  if (base == 0 || current < deepest + kLiveMargin + sizeof(uint64_t)) return;
  const uintptr_t top = (current - kLiveMargin) & ~(uintptr_t)7;
  /* Added, never subtracted: a stack that sits in the first kPaintSpan bytes of the address space —
   * which is where emscripten puts it — makes top - kPaintSpan wrap, and a wrapped bottom is above
   * top, so nothing is painted and every later Mark reports the floor as the peak. */
  const uintptr_t bottom = top >= deepest + kPaintSpan ? top - kPaintSpan : deepest;
  for (uintptr_t a = bottom; a < top; a += sizeof(uint64_t)) *(uint64_t *)a = kPaint;
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

/* LINEAR FROM THE BOTTOM, and not a bisection over it. A frame may reserve bytes it never writes, so
 * an intact stretch can sit above the deepest address ever written; a bisection would take that
 * stretch for the frontier and report LESS stack than was used, which is the wrong direction for a
 * number a stack size gets set from. The first written word from below is the frontier, exactly. */
void StackProbe::Mark() {
  if (tBase == 0) return;
  uintptr_t frontier = tPaintTop;
  for (uintptr_t a = tPaintBottom; a < tPaintTop; a += sizeof(uint64_t))
    if (*(const uint64_t *)a != kPaint) { frontier = a; break; }
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
  }
  return "";
}

} // namespace outshine
