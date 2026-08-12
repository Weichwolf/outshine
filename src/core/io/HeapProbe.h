/* WHAT THE PROCESS HOLDS AND WHAT IT HAS TAKEN, which are two questions and were one column. Only
 * the first can ever fall, which makes it the only one an evictor can be judged by. */
#ifndef HEAPPROBE_H
#define HEAPPROBE_H

#include <cstddef>

namespace outshine {

class HeapProbe {
public:
  /* Bytes the allocator is currently holding out to callers. The allocator answers by walking its
   * zone, so this is O(chunks) and not a per-frame call. */
  static size_t LiveBytes();

  /* What the allocator has taken from the system and holds against future demand. It never falls,
   * so it says how much has been asked for and cannot say what is live. */
  static size_t BreakBytes();

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
