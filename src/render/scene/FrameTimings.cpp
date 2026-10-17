#include "SceneRenderer.h"
#include "Log.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ratio>
#include <string>

namespace outshine::Render {
namespace {
constexpr double kNanosecondsPerMs = 1'000'000.0;
}

void SceneRenderer::CollectGpuTime(Native::Completion completion) {
  if (completion.Device == MeasurementBegin_.Device &&
      completion.Serial <= MeasurementBegin_.Serial) {
    return;
  }
  const auto began = std::chrono::steady_clock::now();
  const auto result = Device_->SubmissionGpuTime(completion);
  Costs_.GpuQueryMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  if (!result || result->Status != Native::CompletionStatus::Ready ||
      !std::isfinite(result->ElapsedNanoseconds) || result->ElapsedNanoseconds < 0) {
    ++Costs_.UnavailableGpuFrames;
    return;
  }
  const double elapsedMs = result->ElapsedNanoseconds / kNanosecondsPerMs;
  ++Costs_.Gpu.Frames;
  Costs_.Gpu.TotalMs += elapsedMs;
  Costs_.Gpu.MostMs = std::max(Costs_.Gpu.MostMs, elapsedMs);
}

void SceneRenderer::ResetMeasurements() noexcept {
  MeasurementBegin_ = LastFrameSubmission_;
  Costs_ = {};
  LastRenderFrameTiming_ = {};
  SlowestRenderFrameTiming_ = {};
  for (auto &stage : Spent_) { stage.TookMs = 0; }
}

void SceneRenderer::CollectCompletedFrameTimes() {
  for (auto &completion : Landed_) {
    if (!completion) { continue; }
    const auto ready = Device_->Poll(*completion);
    if (ready && *ready == Native::CompletionStatus::Ready) {
      CollectGpuTime(*completion);
      completion.reset();
    }
  }
}

Native::Result<void> SceneRenderer::WaitForFrameSlot() {
  auto &completion = Landed_[LandedAt_];
  if (!completion) { return {}; }
  const auto ready = Submission_.WaitFence(
      Submission_.Context, Device_.get(), *completion, std::chrono::seconds(5));
  if (!ready) { return std::unexpected(ready.error()); }
  if (*ready != Native::CompletionStatus::Ready) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Busy,
                      .Message = "GPU frame completion exceeds the explicit wait budget"});
  }
  CollectGpuTime(*completion);
  completion.reset();
  return {};
}

void SceneRenderer::CommitCostProfile(const RenderFrameTiming &timing) {
  ++Costs_.Frames;
  Costs_.HostTotals.TotalMs += timing.TotalMs;
  for (size_t at = 0; at < timing.PhaseMs.size(); ++at) {
    Costs_.HostTotals.PhaseMs[at] += timing.PhaseMs[at];
  }
  Spent_ = PendingSpent_;
  for (size_t at = 0; at < Spent_.size(); ++at) {
    if (Spent_[at].TookMs == 0.0) { continue; }
    auto &cost = Costs_.Stages[at];
    ++cost.Frames;
    cost.TotalMs += Spent_[at].TookMs;
    cost.MostMs = std::max(cost.MostMs, Spent_[at].TookMs);
  }
}

void SceneRenderer::WaitForGpu() {
  if (!Device_) { return; }
  std::string error;
  if (!Settle(error)) {
    Log::Error(LogTag::Render, "gpu_settle_failed", {{"msg", error}});
    return;
  }
  for (auto &completion : Landed_) { completion.reset(); }
}

}
