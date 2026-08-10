/* WHAT A FIXED LINEAR MEMORY DOES WHEN IT IS FULL. There is no growth to fall back on and no second
 * heap to try, so the only honest answers are "the bytes" or "the run ends" — and a run that ends
 * has to say what it was taking and how much, or the next round has a crash and no suspect.
 *
 * This is also the reason the modules link with malloc returning null instead of aborting inside
 * itself: the toolchain's own abort knows the byte count and nothing else. */
#ifndef HEAP_H
#define HEAP_H

#include <cstddef>

namespace outshine {

class Heap {
public:
  /* Never null. `item` is what the caller wanted the bytes for and stands beside the count in the
   * abort line, so it is a literal at the call site rather than a formatted string. */
  static void *Take(const char *item, size_t bytes);
};

} // namespace outshine
#endif
