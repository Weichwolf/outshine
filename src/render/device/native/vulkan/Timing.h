#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_TIMING_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_TIMING_H

#ifdef __linux__
#include "Context.h"
#include <chrono>
#include <cstdint>
#include <optional>

namespace outshine::Render::Native::Vulkan {

class TimingQueries final {
public:
  explicit TimingQueries(Context &context) : Context_(context) {}

  ~TimingQueries();
  TimingQueries(const TimingQueries &) = delete;
  TimingQueries &operator=(const TimingQueries &) = delete;

  [[nodiscard]] Result<void> BeginSubmission(VkCommandBuffer commands);
  void EndSubmission(VkCommandBuffer commands) const;
  [[nodiscard]] Result<SubmissionGpuTiming> ReadSubmission() const;

  [[nodiscard]] Result<std::optional<uint32_t>> Begin(VkCommandBuffer commands);
  void End(VkCommandBuffer commands, std::optional<uint32_t> query) const;
  [[nodiscard]] GpuTimingResults Read() const;

private:
  static constexpr uint32_t kSubmissionQueries = 2;
  static constexpr uint32_t kMaximumDispatches = 128;
  Context &Context_;
  VkQueryPool Pool_ = VK_NULL_HANDLE;
  uint64_t DispatchCount_ = 0;
  std::chrono::steady_clock::time_point RecordedAt_;
};

}
#endif
#endif
