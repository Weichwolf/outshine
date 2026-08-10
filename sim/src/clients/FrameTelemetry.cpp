#include "FrameTelemetry.h"

#include <algorithm>

namespace outshine::Clients {
namespace {

using Pass = Render::GpuTimer;

const char *kPassName[Pass::kPassCount] = {"computeMs", "shadowMs", "sceneMs",   "cloudMs",
                                           "aoMs",      "taaMs",    "presentMs", "overlayMs"};

double Percentile(const std::vector<double> &sorted, double p) {
  if (sorted.empty()) return 0.0;
  return sorted[(size_t)(p * (double)(sorted.size() - 1) + 0.5)];
}

}  // namespace

void FrameTelemetry::AddFrame(double frameMs) { FrameMs_.push_back(frameMs); }

void FrameTelemetry::AddStages(const double ms[Pass::kPassCount]) {
  for (int i = 0; i < Pass::kPassCount; i++) {
    if (ms[i] < 0.0) continue;   /* the pass did not run in that frame */
    StageSum_[i] += ms[i];
    StageN_[i]++;
  }
}

void FrameTelemetry::Reset(double nowMs) {
  FrameMs_.clear();
  for (int i = 0; i < Pass::kPassCount; i++) { StageSum_[i] = 0.0; StageN_[i] = 0; }
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

  int samples = 0;
  for (int i = 0; i < Pass::kPassCount; i++) samples += StageN_[i];
  const bool have = GpuAvailable_ && samples > 0;
  row.Push(std::string(have ? "ok" : GpuAvailable_ ? "pending" : "absent"));
  double sum = 0.0;
  for (int i = 0; i < Pass::kPassCount; i++) {
    if (!have || StageN_[i] == 0) { row.Push(std::string()); continue; }
    const double mean = StageSum_[i] / (double)StageN_[i];
    sum += mean;
    row.Push(mean);
  }
  if (have) row.Push(sum);
  else row.Push(std::string());
}

}  // namespace outshine::Clients
