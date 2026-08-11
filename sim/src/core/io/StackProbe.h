/* HOW MUCH STACK A THREAD ACTUALLY USED, measured rather than assumed: every per-thread stack comes
 * out of the same heap, so a stack size set per purpose is a memory decision and needs a number.
 *
 * The method is painting: at thread entry the free part of the stack is filled with a pattern, and
 * the deepest address that no longer carries it is the deepest the thread ever went. No probe has to
 * be placed at the deepest call, because the stack itself remembers where that was. */
#ifndef STACKPROBE_H
#define STACKPROBE_H

#include <atomic>
#include <cstddef>

namespace outshine {

class StackProbe {
public:
  /* A stack per purpose, not per thread: the compute pool has as many threads as the host has cores,
   * and a schema whose column count follows the host is not a schema. Threads sharing a purpose
   * publish the deepest of them. */
  enum class Purpose { Frame, Class, Tile, Region };
  static constexpr int kPurposeCount = 4;

  /* Paints part of this thread's free stack and binds it to `purpose`. Called once, as early in the
   * thread as the caller can manage: whatever is already on the stack is invisible to the probe and
   * is published as its floor. */
  static void Enter(Purpose purpose);
  /* Scans this thread's own stack and publishes the deepest point it has reached. Only the owning
   * thread may call it, which is what keeps the cross-thread traffic to the published atomic. */
  static void Mark();

  static size_t PeakBytes(Purpose purpose);
  static size_t CapacityBytes(Purpose purpose);
  /* THE PROBE'S OWN RANGE, so a peak is falsifiable rather than merely plausible. `Floor` is what was
   * already on the stack when Enter ran plus its safety margin — below that the probe sees nothing,
   * and a peak sitting exactly there means the thread never went deeper than its entry point.
   * `Limit` is the deepest it can see; a peak reaching it is a lower bound, not a measurement. */
  static size_t FloorBytes(Purpose purpose);
  static size_t LimitBytes(Purpose purpose);
  static const char *Name(Purpose purpose);
};

} // namespace outshine
#endif
