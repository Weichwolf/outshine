/* WHAT THE CLIENT HOLDS AND WHAT IT HAS TAKEN, which are two questions and were one column. wasm32
 * gives one address space and the graphics API refuses to grow it, so the fixed heap has to be set
 * against the second — and only the first can ever fall, which makes it the only one an evictor can
 * be judged by. */
#ifndef HEAPPROBE_H
#define HEAPPROBE_H

#include <cstddef>

namespace outshine {

class HeapProbe {
public:
  /* Bytes the allocator is currently holding out to callers. dlmalloc answers by walking every
   * chunk in every segment, so this is O(chunks) and not a per-frame call. */
  static size_t LiveBytes();
  /* Whether the allocator answers that question at all on this platform. False leaves the live side
   * EMPTY in the ledger, because an unmeasured quantity is not a zero. */
  static bool LiveBytesKnown();

  /* What the allocator has taken from the system and holds against future demand. Under wasm this
   * is the program break, and a break never falls: it is how close the fixed heap is to its ceiling
   * and it cannot answer what is live. */
  static size_t BreakBytes();

  /* The linear memory itself — what the module reserved at instantiation and, the heap being fixed,
   * for as long as it runs. Read off the module rather than declared a second time here. 0 where
   * there is no such thing, which is every native host. */
  static size_t ReservedBytes();

  /* The largest LiveBytes() any Sample() has seen, so the resolution of the peak is the sampling
   * rate and nothing finer. */
  static size_t PeakLiveBytes();
  /* What the last Sample() spent inside LiveBytes(). The price of this instrument is a function of
   * the heap it measures, so it is published beside its reading rather than assumed small. */
  static double SampleCostMs();
  /* Updates the peak and hands back the reading it took: a row that has to add up costs one walk
   * and quotes one number in every column derived from it. */
  static size_t Sample();
};

} // namespace outshine
#endif
