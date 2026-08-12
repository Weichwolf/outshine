/* PER-PASS GPU TIME, plus the one span that says whether the per-pass numbers may be attributed at
 * all. A begin/end timestamp pair measures a SPAN on the GPU timeline; pipelined passes overlap, so
 * summing the spans is not the frame — measured natively, the six spans summed to 23.3 ms against a
 * frame p50 of 9.38. The pair therefore licenses exactly one statement, "did this pass's span move
 * between two builds of the same declared scene", and never "this pass costs N ms".
 *
 * `FrameMs` is the discriminator and it costs no query: the first pass's begin and the last pass's
 * end are already written, so the whole encoder's span is the newest end minus the oldest begin.
 * Sum-over-span above 1 means the spans overlap and attribution is forbidden.
 *
 * The pass set is fixed at compile time, so a sample is a fixed row and not a string map, AND the
 * enumeration is the encoder's pass count — a slot with no pass is where a new pass hides without
 * the count moving. Resolve and readback ride a ring deep enough that no frame waits on its own
 * result; WHAT COMES BACK IS ONE TO THREE FRAMES OLD, so a sample belongs to a frame that has
 * already been presented. */
#ifndef GPUTIMER_H
#define GPUTIMER_H

#include <webgpu/webgpu_cpp.h>

namespace outshine::Render {

class GpuTimer {
public:
  /* Order is the ENCODE order, the query order and the log's column order, and the count is the
   * number of passes the encoder opens. Add one, add its Writes() call. */
  enum Pass { Atmosphere, Light, Shadow, Scene, Ao, Taa, Present, kPassCount };

  struct Sample {
    double PassMs[kPassCount];   /* -1 = the pass did not run; never averaged with a 0.0 */
    double FrameMs;              /* the whole encoder's span, or -1 */
  };

  /* NO ENVIRONMENT GATE. An environment variable is not an interface in a browser, and the frame
   * spectrum is telemetry rather than a bench mode: it runs whenever the device grants
   * the feature. A device that does not is reported as such — an absent measurement is not a
   * measurement of zero. */
  void Configure(const wgpu::Device &dev, bool featureGranted);
  [[nodiscard]] bool Active(void) const { return Set_ != nullptr; }

  void BeginFrame(void);
  /* The struct to splice into a pass descriptor's `timestampWrites`, or null when inactive. It lives
   * until the next BeginFrame. */
  wgpu::PassTimestampWrites *Writes(Pass p);

  void Resolve(wgpu::CommandEncoder &enc);   /* after the last pass, before Finish() */
  void Poll(void);                           /* once per frame: maps whatever is ready */

  [[nodiscard]] bool Take(Sample &out);   /* the newest sample, once */

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
  Sample Last_ = {};
  bool Fresh_ = false;
};

} // namespace outshine::Render
#endif
