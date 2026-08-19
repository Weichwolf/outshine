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

  /* **WHAT THIS ENGINE'S OWN `new` HAS TAKEN AND ITS `delete` HAS NOT RETURNED** (board:1462). It is a
   * different population from `HeapProbe::LiveBytes`, which walks the process's default malloc zone and
   * therefore answers for SDL, the driver and every mapped library as well: this counts only blocks
   * that crossed the operators replaced below, so a number drawn from it is one this repository is
   * answerable for.
   *
   * **IT EXISTS TO PROVE A ZERO, NOT TO FIND A LEAK.** Unreal frees a frame's allocations en masse
   * with an `FMemMark` over `FMemStack`; RAGE sizes its pools at build time and refuses rather than
   * grows; `CLAUDE.md` says the same thing in its own words -- an allocation is not a bounded term, so
   * it lives at load. **A frame path that takes nothing cannot leak, cannot fragment and cannot stall
   * in an allocator**, and equality between two frames then follows by construction. This is the
   * instrument that says whether the tree obeys its own rule.
   *
   * The read is one relaxed load and the accounting is two adds, so nothing here walks anything. */
  static size_t LiveBytes();

  /* A refusal that happened where the count is not ours to see — a C module that answers "out of
   * memory" with an error code. Without this the code arrives as a gap in the world and the run
   * carries on with a hole nobody can trace back to the allocator. */
  [[noreturn]] static void Exhausted(const char *item);
};

} // namespace outshine
#endif
