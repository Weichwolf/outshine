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
 * TWO QUANTITIES, AGGREGATED DIFFERENTLY ON PURPOSE:
 *   per stage   MEAN over the second — the question is where the time goes, and a share is a mean
 *   whole frame DISTRIBUTION, p50/p95/p99 over the second's frames — the question is whether it
 *               stutters, and a mean is precisely what hides that (CLAUDE.md)
 *
 * GRADED HONESTY. The frame distribution costs a clock read and is always there. The stage times
 * need `timestamp-query`, which in Chrome needs --enable-dawn-features=allow_unsafe_apis; where the
 * device does not grant it the row says `gpu=absent` and the stage columns stay empty. An absent
 * measurement is not a measurement of zero.
 *
 * A GPU SAMPLE IS ONE TO THREE FRAMES OLD (GpuTimer.h): the readback ring is three deep. Over a
 * one-second window that shifts the stage means by at most three frames of the window's edge and is
 * immaterial; no row claims to attribute a single frame's hitch to a single sample. */
class FrameTelemetry : public TelemetrySource {
public:
  static constexpr double kWindowMs = 1000.0;

  void SetGpuAvailable(bool on) { GpuAvailable_ = on; }
  void AddFrame(double frameMs);
  void AddStages(const double ms[Render::GpuTimer::kPassCount]);
  bool Due(double nowMs) const { return nowMs - StartedMs_ >= kWindowMs && !FrameMs_.empty(); }
  void Open(double nowMs) { StartedMs_ = nowMs; }
  void Reset(double nowMs);

  const char *TelemetryName() const override { return "frame"; }
  void DeclareTelemetry(TelemetrySchema &schema) const override;
  void SampleTelemetry(TelemetryRow &row) const override;

private:
  std::vector<double> FrameMs_;
  double StageSum_[Render::GpuTimer::kPassCount] = {};
  int StageN_[Render::GpuTimer::kPassCount] = {};
  double StartedMs_ = 0.0;
  bool GpuAvailable_ = false;
};

} // namespace outshine::Clients
#endif
