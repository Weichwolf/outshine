#ifdef __linux__
#include "Timing.h"
#include "TimestampRange.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <ratio>

namespace outshine::Render::Native::Vulkan {

TimingQueries::~TimingQueries() {
  if (Pool_ != VK_NULL_HANDLE) { vkDestroyQueryPool(Context_.Logical, Pool_, nullptr); }
}

Result<std::optional<uint32_t>> TimingQueries::Begin(VkCommandBuffer commands) {
  if (!Context_.GpuTiming) { return std::nullopt; }
  if (Pool_ == VK_NULL_HANDLE) {
    VkQueryPoolCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    create.queryType = VK_QUERY_TYPE_TIMESTAMP;
    create.queryCount = 2 * kMaximumDispatches;
    const auto result = vkCreateQueryPool(Context_.Logical, &create, nullptr, &Pool_);
    if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateQueryPool", result)); }
    RecordedAt_ = std::chrono::steady_clock::now();
    vkCmdResetQueryPool(commands, Pool_, 0, create.queryCount);
  }
  const uint64_t ordinal = DispatchCount_++;
  if (ordinal >= kMaximumDispatches) { return std::nullopt; }
  const auto query = static_cast<uint32_t>(ordinal) * 2;
  vkCmdWriteTimestamp2(commands, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, Pool_, query);
  return query;
}

void TimingQueries::End(VkCommandBuffer commands, std::optional<uint32_t> query) const {
  if (query) {
    vkCmdWriteTimestamp2(commands, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, Pool_, *query + 1);
  }
}

GpuTimingResults TimingQueries::Read() const {
  GpuTimingResults timings{.Status = CompletionStatus::Ready,
                           .DispatchCount = DispatchCount_,
                           .Truncated = DispatchCount_ > kMaximumDispatches,
                           .Dispatches = {}};
  const auto count = static_cast<uint32_t>(std::min<uint64_t>(DispatchCount_, kMaximumDispatches));
  if (count == 0) { return timings; }
  std::array<uint64_t, size_t{kMaximumDispatches} * 4> values{};
  const auto result =
      vkGetQueryPoolResults(Context_.Logical,
                            Pool_,
                            0,
                            count * 2,
                            sizeof(values),
                            values.data(),
                            2 * sizeof(uint64_t),
                            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
  const bool readable = result == VK_SUCCESS || result == VK_NOT_READY;
  const double hostUpperBoundNs =
      std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - RecordedAt_)
          .count();
  timings.Dispatches.reserve(count);
  for (uint32_t index = 0; index < count; ++index) {
    const auto at = size_t{index} * 4;
    const bool available = readable && values[at + 1] != 0 && values[at + 3] != 0 &&
                           TimestampIntervalFits(values[at],
                                                 values[at + 2],
                                                 Context_.TimestampValidBits,
                                                 Context_.Properties.limits.timestampPeriod,
                                                 hostUpperBoundNs);
    timings.Dispatches.push_back({.Ordinal = index,
                                  .Begin = available ? values[at] : 0,
                                  .End = available ? values[at + 2] : 0,
                                  .NanosecondsPerTick = Context_.Properties.limits.timestampPeriod,
                                  .ValidBits = Context_.TimestampValidBits,
                                  .Available = available});
  }
  return timings;
}

}
#endif
