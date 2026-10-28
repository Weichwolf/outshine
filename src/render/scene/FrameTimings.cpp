#include "SceneRenderer.h"
#include "GpuFrameSample.h"
#include "Log.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ratio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
constexpr double kNanosecondsPerMs = 1'000'000.0;
}

std::string_view SceneRenderer::GpuName() const noexcept {
  return Device_ ? std::string_view(Device_->Limits().Name) : std::string_view{};
}

void SceneRenderer::CollectGpuTime(Native::Completion completion) {
  if (completion.Device == MeasurementBegin_.Device &&
      completion.Serial <= MeasurementBegin_.Serial) {
    return;
  }
  const auto began = std::chrono::steady_clock::now();
  const auto recordCost = [this, began] {
    Costs_.GpuQueryMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  };
  const auto result = Device_->SubmissionGpuTime(completion);
  if (!result || result->Status != Native::CompletionStatus::Ready ||
      !std::isfinite(result->ElapsedNanoseconds) || result->ElapsedNanoseconds < 0) {
    ++Costs_.UnavailableGpuFrames;
    recordCost();
    return;
  }
  const double elapsedMs = result->ElapsedNanoseconds / kNanosecondsPerMs;
  ++Costs_.Gpu.Frames;
  Costs_.Gpu.TotalMs += elapsedMs;
  Costs_.Gpu.MostMs = std::max(Costs_.Gpu.MostMs, elapsedMs);
  if (GpuSampleLimit_ != 0) {
    if (GpuSamples_.size() < GpuSampleLimit_) {
      GpuSamples_.push_back(
          {.Device = completion.Device, .Submission = completion.Serial, .ElapsedMs = elapsedMs});
    } else {
      ++Costs_.DroppedGpuSamples;
    }
  }
  recordCost();
}

void SceneRenderer::KeepGpuSamples(size_t maximum) {
  std::vector<GpuFrameSample> samples;
  samples.reserve(maximum);
  GpuSamples_ = std::move(samples);
  GpuSampleLimit_ = maximum;
}

void SceneRenderer::ResetMeasurements() noexcept {
  MeasurementBegin_ = Frames_.LastSubmitted();
  Costs_ = {};
  GpuSamples_.clear();
  LastRenderFrameTiming_ = {};
  SlowestRenderFrameTiming_ = {};
  for (auto &stage : Spent_) { stage.TookMs = 0; }
}

void SceneRenderer::CollectCompletedFrameTimes() {
  for (const auto &completion : Frames_.Pending()) {
    if (!completion) { continue; }
    const auto began = std::chrono::steady_clock::now();
    const auto ready = Device_->Poll(*completion);
    Costs_.GpuQueryMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    if (ready && *ready == Native::CompletionStatus::Ready) {
      CollectGpuTime(*completion);
      Frames_.Retire(*completion);
    }
  }
}

Native::Result<void> SceneRenderer::WaitForFrameSlot() {
  const auto completion = Frames_.Next();
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
  Frames_.Retire(*completion);
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
  const bool settled = Settle(error);
  if (!settled) { Log::Error(LogTag::Render, "gpu_settle_failed", {{"msg", error}}); }
  if (MaterialFields_) {
    const auto retired = MaterialFields_->Collect();
    if (!retired) {
      Log::Error(
          LogTag::Render, "material_field_retire_failed", {{"msg", retired.error().Message}});
    }
  }
  if (settled) { Frames_.RetireAll(); }
}

}
