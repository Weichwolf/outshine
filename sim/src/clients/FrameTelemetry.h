#ifndef FRAMETELEMETRY_H
#define FRAMETELEMETRY_H

#include <string>
#include <vector>

#include "GpuTimer.h"
#include "Telemetry.h"

namespace outshine::Clients {

/* THE FPS SPECTRUM, ONE ROW PER SECOND, and it is telemetry rather than a bench: a running client
 * is observable by itself, without anyone declaring a measurement.
 *
 * EVERY QUANTITY IS A DISTRIBUTION and is published as its median over the second — a mean hides
 * exactly the stutter these rows exist to show (CLAUDE.md), and that holds for a pass's span as much
 * as for the frame.
 *
 * A PASS SPAN IS NOT A SHARE OF THE FRAME. Pipelined passes overlap, so `gpuSumMs / gpuFrameMs`
 * above 1 says the spans overlap and no pass may be given a share; the ratio is published beside
 * them so a reader cannot take the sum for a partition (GpuTimer.h).
 *
 * GRADED HONESTY. The frame distribution costs a clock read and is always there. The pass spans need
 * `timestamp-query`, which in Chrome needs --enable-dawn-features=allow_unsafe_apis; where the
 * device does not grant it the row says `gpu=absent` and the pass columns stay empty. An absent
 * measurement is not a measurement of zero.
 *
 * A GPU SAMPLE IS ONE TO THREE FRAMES OLD (GpuTimer.h): the readback ring is three deep. Over a
 * one-second window that shifts the medians by at most three frames of the window's edge and is
 * immaterial; no row claims to attribute a single frame's hitch to a single sample. */
class FrameTelemetry : public TelemetrySource {
public:
  static constexpr double kWindowMs = 1000.0;

  void SetGpuAvailable(bool on) { GpuAvailable_ = on; }
  void AddFrame(double frameMs);
  void AddGpu(const Render::GpuTimer::Sample &sample);
  bool Due(double nowMs) const { return nowMs - StartedMs_ >= kWindowMs && !FrameMs_.empty(); }
  void Open(double nowMs) { StartedMs_ = nowMs; }
  void Reset(double nowMs);

  const char *TelemetryName() const override { return "frame"; }
  void DeclareTelemetry(TelemetrySchema &schema) const override;
  void SampleTelemetry(TelemetryRow &row) const override;

private:
  std::vector<double> FrameMs_;
  std::vector<double> PassMs_[Render::GpuTimer::kPassCount];
  std::vector<double> GpuFrameMs_;
  double StartedMs_ = 0.0;
  bool GpuAvailable_ = false;
};

} // namespace outshine::Clients
#endif
