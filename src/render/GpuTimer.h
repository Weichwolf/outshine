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
 * THE SLOT COUNT AND THE SLOT NAMES COME FROM THE COMPILED PLAN, so there is no tally to keep and no
 * dead slot for a pass to hide in: the number is written down once, by the compiler that derived it.
 * The array is bounded by the catalogue, because a plan can never hold more passes than it has
 * stages. Resolve and readback ride a ring deep enough that no frame waits on its own result; WHAT
 * COMES BACK IS ONE TO THREE FRAMES OLD, so a sample belongs to a frame already presented. */
#ifndef GPUTIMER_H
#define GPUTIMER_H

#include <webgpu/webgpu_cpp.h>

#include "RenderCatalogue.h"

namespace outshine::Render {

class GpuTimer {
public:
  /* A plan holds at most one pass per stage it holds, so the catalogue bounds the row. */
  static constexpr int kMaxPasses = (int)kStageCount;

  struct Sample {
    double PassMs[kMaxPasses];   /* -1 = the pass did not run; never averaged with a 0.0 */
    double FrameMs;              /* the whole encoder's span, or -1 */
    int PassCount;               /* what the compiled plan derived */
  };

  /* NO ENVIRONMENT GATE. An environment variable is not an interface in a browser, and the frame
   * spectrum is telemetry rather than a bench mode: it runs whenever the device grants
   * the feature. A device that does not is reported as such — an absent measurement is not a
   * measurement of zero. */
  void Configure(const wgpu::Device &dev, bool featureGranted, int passCount);
  [[nodiscard]] bool Active(void) const { return Set_ != nullptr; }

  void BeginFrame(void);
  /* The struct to splice into a pass descriptor's `timestampWrites`, or null when inactive. It lives
   * until the next BeginFrame. */
  wgpu::PassTimestampWrites *Writes(int pass);

  void Resolve(wgpu::CommandEncoder &enc);   /* after the last pass, before Finish() */
  void Poll(void);                           /* once per frame: maps whatever is ready */

  [[nodiscard]] bool Take(Sample &out);   /* the newest sample, once */

private:
  static constexpr int kRing = 3;   /* deep enough that a map never blocks the frame that issued it */
  static constexpr int kQueries = kMaxPasses * 2;

  wgpu::Device Dev_;
  wgpu::QuerySet Set_;
  wgpu::Buffer Resolved_;
  wgpu::Buffer Read_[kRing];
  bool Busy_[kRing] = {};
  int Slot_ = 0;
  wgpu::PassTimestampWrites Writes_[kMaxPasses];
  bool Used_[kMaxPasses] = {};
  int Passes_ = 0;
  Sample Last_ = {};
  bool Fresh_ = false;
};

} // namespace outshine::Render
#endif
