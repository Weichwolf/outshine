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

  /* **WHAT THE ALLOCATIONS INSIDE A SCOPE ARE FOR** (board:1463). Unreal calls this a Low Level Memory
   * Tracker and it is the established answer to *which call site took that*: a scope names a category
   * and every allocation under it is attributed there.
   *
   * **IT COUNTS WHAT WAS TAKEN AND NEVER WHAT IS LIVE, which is what keeps it free.** Attributing a
   * RETURN needs the tag stored beside the block, and a table from pointer to tag is an allocation on
   * the free path -- the defect this whole item removes, wearing the costume of the fix. A monotone
   * per-tag total answers *where does this frame's traffic come from*, which is the question, and the
   * net is already answered by `LiveBytes`.
   *
   * The tag is a LITERAL and is compared by pointer, so entering a scope is one store and leaving it
   * is one more. A tag the table has no room for is counted under `other` rather than dropped. */
  class Tagged {
  public:
    explicit Tagged(const char *tag) noexcept;
    ~Tagged() noexcept;
    Tagged(const Tagged &) = delete;
    Tagged &operator=(const Tagged &) = delete;

  private:
    const char *Held_;
  };

  /* Bytes taken under that tag since the program began. Monotone, so a caller reads it twice and
   * subtracts. An unknown tag answers zero rather than refusing: this is a diagnostic. */
  [[nodiscard]] static size_t TakenUnder(const char *tag);
  /* How many tags the table holds, so a consumer can enumerate what was actually seen. */
  [[nodiscard]] static size_t TagCount();
  [[nodiscard]] static const char *TagAt(size_t at);
  [[nodiscard]] static size_t TakenAt(size_t at);

  /* A refusal that happened where the count is not ours to see — a C module that answers "out of
   * memory" with an error code. Without this the code arrives as a gap in the world and the run
   * carries on with a hole nobody can trace back to the allocator. */
  [[noreturn]] static void Exhausted(const char *item);
};

} // namespace outshine
#endif
