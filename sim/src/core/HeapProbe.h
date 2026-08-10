/* HOW MUCH LINEAR MEMORY THE CLIENT HOLDS, and the most it has ever held. wasm32 gives one address
 * space and the graphics API refuses to grow it, so this is the number a fixed heap has to be set
 * from — and until it exists no statement about the memory budget has provenance. */
#ifndef HEAPPROBE_H
#define HEAPPROBE_H

#include <cstddef>

namespace outshine {

class HeapProbe {
public:
  /* In the browser: everything below the allocator's break — static data, every thread's stack and
   * the heap — because that is exactly what a fixed linear memory has to cover. Natively the same
   * question is answered as tightly as the platform allows, which is the default zone's live bytes;
   * the two are the same quantity to within what sits outside a malloc arena. */
  static size_t Bytes();
  /* The most Bytes() has been at any Sample(), which is the frame rate — the quantity only grows in
   * the browser, but the peak is what the ceiling is read against and it is stated as one. */
  static size_t PeakBytes();
  static void Sample();

  /* The wall, read off the link rather than declared a second time here: once the heap is fixed this
   * IS the linear memory, and a growing one reports the maximum it may reach. 0 where the platform
   * has no such wall, which is every native host. */
  static size_t CeilingBytes();
};

} // namespace outshine
#endif
