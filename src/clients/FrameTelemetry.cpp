#include "FrameTelemetry.h"

#include <algorithm>

namespace outshine::Clients {
namespace {

using Pass = Render::GpuTimer;

const char *kPassName[Pass::kPassCount] = {"atmosphereMs", "lightMs", "shadowMs", "sceneMs",
                                           "aoMs",         "taaMs",   "presentMs"};

double Percentile(const std::vector<double> &sorted, double p) {
  if (sorted.empty()) return 0.0;
  return sorted[(size_t)(p * (double)(sorted.size() - 1) + 0.5)];
}

double Median(std::vector<double> v) {
  std::sort(v.begin(), v.end());
  return Percentile(v, 0.50);
}

}  // namespace

void FrameTelemetry::AddFrame(double frameMs) { FrameMs_.push_back(frameMs); }

void FrameTelemetry::AddGpu(const Render::GpuTimer::Sample &sample) {
  for (int i = 0; i < Pass::kPassCount; i++) {
    if (sample.PassMs[i] < 0.0) continue;   /* the pass did not run in that frame */
    PassMs_[i].push_back(sample.PassMs[i]);
  }
  if (sample.FrameMs >= 0.0) GpuFrameMs_.push_back(sample.FrameMs);
}

void FrameTelemetry::Reset(double nowMs) {
  FrameMs_.clear();
  for (int i = 0; i < Pass::kPassCount; i++) PassMs_[i].clear();
  GpuFrameMs_.clear();
  StartedMs_ = nowMs;
}

void FrameTelemetry::DeclareTelemetry(TelemetrySchema &schema) const {
  schema.Add("frames");
  schema.Add("windowMs", "ms");
  schema.Add("fps", "1/s");
  schema.Add("p50Ms", "ms");
  schema.Add("p95Ms", "ms");
  schema.Add("p99Ms", "ms");
  schema.Add("maxMs", "ms");
  schema.Add("gpu");
  for (int i = 0; i < Pass::kPassCount; i++) schema.Add(kPassName[i], "ms");
  schema.Add("gpuSumMs", "ms");
  schema.Add("gpuFrameMs", "ms");
  schema.Add("gpuSpanRatio");
}

void FrameTelemetry::SampleTelemetry(TelemetryRow &row) const {
  std::vector<double> sorted = FrameMs_;
  std::sort(sorted.begin(), sorted.end());
  double span = 0.0;
  for (double v : FrameMs_) span += v;
  row.Push((int)FrameMs_.size());
  row.Push(span);
  row.Push(span > 0.0 ? 1000.0 * (double)FrameMs_.size() / span : 0.0);
  row.Push(Percentile(sorted, 0.50));
  row.Push(Percentile(sorted, 0.95));
  row.Push(Percentile(sorted, 0.99));
  row.Push(sorted.empty() ? 0.0 : sorted.back());

  size_t samples = 0;
  for (int i = 0; i < Pass::kPassCount; i++) samples += PassMs_[i].size();
  const bool have = GpuAvailable_ && samples > 0;
  row.Push(std::string(have ? "ok" : GpuAvailable_ ? "pending" : "absent"));
  double sum = 0.0;
  for (int i = 0; i < Pass::kPassCount; i++) {
    if (!have || PassMs_[i].empty()) { row.Push(std::string()); continue; }
    const double p50 = Median(PassMs_[i]);
    sum += p50;
    row.Push(p50);
  }
  if (!have) {
    row.Push(std::string());
    row.Push(std::string());
    row.Push(std::string());
    return;
  }
  const double frame = GpuFrameMs_.empty() ? 0.0 : Median(GpuFrameMs_);
  row.Push(sum);
  row.Push(frame);
  /* ABOVE 1 THE SPANS OVERLAP and no pass may be given a share of the frame. */
  row.Push(frame > 0.0 ? sum / frame : 0.0);
}

}  // namespace outshine::Clients
