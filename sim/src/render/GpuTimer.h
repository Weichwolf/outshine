/* PER-PASS GPU TIME. Without it the passes are one number, and the frame's dominant cost — a p50 of
 * 6.9 ms against a p99 of 16.1 ms on a camera that moves seven metres in 300 frames — cannot be
 * attributed to any of them. A diagnostic and not a mode: nothing is allocated and no timestamp is
 * written unless FB_GPUTIME is set, and the rendered frame is bit-identical either way.
 *
 * The pass set is fixed at compile time, so a sample is a fixed row and not a string map. Resolve and
 * readback ride a ring deep enough that no frame waits on its own result; what comes back is a few
 * frames old, which is what a distribution wants anyway. */
#ifndef GPUTIMER_H
#define GPUTIMER_H

#include <webgpu/webgpu_cpp.h>

namespace outshine::Render {

class GpuTimer {
public:
  /* Order is the query order and the log's column order. Add one, add its Writes() call. */
  enum Pass { Compute, Shadow, Scene, Cloud, Ao, Taa, Tonemap, Overlay, kPassCount };

  static bool Wanted(void);   /* FB_GPUTIME, read once */

  void Configure(const wgpu::Device &dev);
  bool Active(void) const { return Set_ != nullptr; }

  void BeginFrame(void);
  /* The struct to splice into a pass descriptor's `timestampWrites`, or null when inactive. It lives
   * until the next BeginFrame. */
  wgpu::PassTimestampWrites *Writes(Pass p);

  void Resolve(wgpu::CommandEncoder &enc);   /* after the last pass, before Finish() */
  void Poll(void);                           /* once per frame: maps what is ready and logs it */

private:
  static constexpr int kRing = 3;   /* deep enough that a map never blocks the frame that issued it */
  static constexpr int kQueries = kPassCount * 2;

  wgpu::Device Dev_;
  wgpu::QuerySet Set_;
  wgpu::Buffer Resolved_;
  wgpu::Buffer Read_[kRing];
  bool Busy_[kRing] = {};
  int Slot_ = 0;
  wgpu::PassTimestampWrites Writes_[kPassCount];
  bool Used_[kPassCount] = {};
};

} // namespace outshine::Render
#endif
