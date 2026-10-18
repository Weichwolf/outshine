#ifndef OUTSHINE_TEST_VULKAN_WORK_PROFILE_H
#define OUTSHINE_TEST_VULKAN_WORK_PROFILE_H
#include "Context.h"
#include "RenderState.h"
#include "TimestampRange.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Render::Native::Vulkan::WorkProbe {
constexpr uint32_t kSlots = 256;
constexpr uint32_t kAbsent = std::numeric_limits<uint32_t>::max();
constexpr size_t kCaptureLimit = 256;
using Clock = std::chrono::steady_clock;
inline double CpuNanoseconds = 0;
inline uint64_t CpuCalls = 0;

struct CpuScope {
  Clock::time_point Begin = Clock::now();

  ~CpuScope() {
    CpuNanoseconds += std::chrono::duration<double, std::nano>(Clock::now() - Begin).count();
    ++CpuCalls;
  }
};

struct PipelineInfo {
  VkDevice Device = VK_NULL_HANDLE;
  std::string Label;
  bool Fragment = false;
};

struct Sample {
  std::string Label;
  uint32_t Parent = kAbsent;
  uint32_t Kind = 0;
  size_t Calls = 0;
  Workgroups Groups{};
  bool Fragment = false;
  bool Missing = false;
  bool Details = false;
};

struct Trace {
  VkDevice Device = VK_NULL_HANDLE;
  VkQueryPool Times = VK_NULL_HANDLE;
  VkQueryPool Statistics = VK_NULL_HANDLE;
  Clock::time_point Began = Clock::now();
  uint64_t Serial = 0;
  uint32_t Count = 0;
  uint32_t Omitted = 0;
  double SubmissionNs = -1;
  double Period = 0;
  uint32_t Bits = 0;
  double HostBoundNs = 0;
  VkResult StatisticsResult = VK_NOT_READY;
  VkResult TimestampResult = VK_NOT_READY;
  std::array<Sample, kSlots> Samples;
  std::array<uint64_t, size_t{kSlots} * 6> Counters{};
  std::array<uint64_t, size_t{kSlots} * 4> Timestamps{};
  GpuTimingResults Compute;

  void Release() {
    if (Times != VK_NULL_HANDLE) {
      vkDestroyQueryPool(Device, Times, nullptr);
      Times = VK_NULL_HANDLE;
    }
    if (Statistics != VK_NULL_HANDLE) {
      vkDestroyQueryPool(Device, Statistics, nullptr);
      Statistics = VK_NULL_HANDLE;
    }
  }

  ~Trace() { Release(); }
};

inline std::map<VkPipeline, PipelineInfo> Pipelines;
inline std::map<VkCommandBuffer, std::unique_ptr<Trace>> Traces;
inline std::vector<std::unique_ptr<Trace>> Completed;
inline uint64_t Serial = 0;
inline uint64_t OmittedCaptures = 0;
inline uint64_t CancelledCaptures = 0;

inline void Require(bool valid, std::string_view message) {
  if (!valid) {
    std::println(stderr, "VK_WORK_ERROR\t{}", message);
    std::abort();
  }
}

inline void Register(VkPipeline pipeline, VkDevice device, std::string_view name, bool fragment) {
  const CpuScope cost;
  Require(Pipelines.size() < 4096 || Pipelines.contains(pipeline), "pipeline limit exceeded");
  Require(name.size() <= 1024, "pipeline label byte limit exceeded");
  std::string label(name);
  std::ranges::replace(label, '\t', ' ');
  std::ranges::replace(label, '\n', ' ');
  Pipelines[pipeline] = {.Device = device, .Label = std::move(label), .Fragment = fragment};
}

inline Trace &Start(const Context &context, VkCommandBuffer commands) {
  Require(OnOwner(context), "wrong render thread");
  Require(context.GpuTiming, "work probe requires client --measures");
  if (const auto found = Traces.find(commands); found != Traces.end()) { return *found->second; }
  Require(Traces.size() < 16, "live command limit exceeded");
  auto trace = std::make_unique<Trace>();
  trace->Device = context.Logical;
  trace->Serial = ++Serial;
  trace->Period = context.Properties.limits.timestampPeriod;
  trace->Bits = context.TimestampValidBits;
  VkQueryPoolCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  info.queryCount = 2 * kSlots;
  Require(vkCreateQueryPool(context.Logical, &info, nullptr, &trace->Times) == VK_SUCCESS,
          "timestamp pool creation failed");
  info.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
  info.queryCount = kSlots;
  info.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
                            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
                            VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
                            VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;
  Require(vkCreateQueryPool(context.Logical, &info, nullptr, &trace->Statistics) == VK_SUCCESS,
          "pipeline statistics pool creation failed");
  vkCmdResetQueryPool(commands, trace->Times, 0, 2 * kSlots);
  vkCmdResetQueryPool(commands, trace->Statistics, 0, kSlots);
  auto &result = *trace;
  Traces.emplace(commands, std::move(trace));
  return result;
}

inline uint32_t Allocate(Trace &trace) {
  if (trace.Count == kSlots) {
    ++trace.Omitted;
    return kAbsent;
  }
  return trace.Count++;
}

inline void BeginQuery(Trace &trace, VkCommandBuffer commands, uint32_t slot, bool statistics) {
  vkCmdWriteTimestamp2(commands, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, trace.Times, 2 * slot);
  if (statistics) { vkCmdBeginQuery(commands, trace.Statistics, slot, 0); }
}

inline void EndQuery(Trace &trace, VkCommandBuffer commands, uint32_t slot, bool statistics) {
  if (statistics) { vkCmdEndQuery(commands, trace.Statistics, slot); }
  vkCmdWriteTimestamp2(commands, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, trace.Times, 2 * slot + 1);
}

inline void Identify(Sample &sample, VkPipeline pipeline) {
  const auto found = Pipelines.find(pipeline);
  if (found == Pipelines.end()) {
    sample.Missing = true;
    return;
  }
  if (!sample.Label.empty()) { sample.Label += '|'; }
  sample.Label += found->second.Label;
  sample.Fragment = sample.Fragment || found->second.Fragment;
}

inline uint32_t
Begin(const Context &context, VkCommandBuffer commands, std::span<const DrawPacket> draws) {
  const CpuScope cost;
  auto &trace = Start(context, commands);
  const auto slot = Allocate(trace);
  if (slot == kAbsent) { return slot; }
  auto &sample = trace.Samples[slot];
  sample.Calls = draws.size();
  std::array<VkPipeline, 64> seen{};
  size_t seenCount = 0;
  for (const auto &draw : draws) {
    auto *const handle = draw.Graphics->Pipeline->Handle;
    const auto found = Pipelines.find(handle);
    if (found == Pipelines.end()) {
      sample.Missing = true;
      continue;
    }
    if (!std::ranges::contains(std::span(seen).first(seenCount), handle)) {
      Require(seenCount < seen.size(), "pass pipeline count exceeded");
      seen[seenCount++] = handle;
      Identify(sample, handle);
      Require(sample.Label.size() <= 4096, "pass label byte limit exceeded");
    }
  }
  sample.Details = sample.Label.contains("groundPatchLattice") && !sample.Missing;
  BeginQuery(trace, commands, slot, !sample.Details);
  return slot;
}

inline uint32_t BeginDraw(VkCommandBuffer commands, uint32_t parent, const DrawPacket &draw) {
  const CpuScope cost;
  if (parent == kAbsent) { return kAbsent; }
  auto &trace = *Traces.at(commands);
  if (!trace.Samples[parent].Details) { return kAbsent; }
  const auto slot = Allocate(trace);
  if (slot == kAbsent) { return slot; }
  auto &sample = trace.Samples[slot];
  sample.Parent = parent;
  sample.Kind = 1;
  sample.Calls = 1;
  Identify(sample, draw.Graphics->Pipeline->Handle);
  BeginQuery(trace, commands, slot, true);
  return slot;
}

inline uint32_t BeginCompute(const Context &context,
                             VkCommandBuffer commands,
                             VkPipeline pipeline,
                             Workgroups groups) {
  const CpuScope cost;
  auto &trace = Start(context, commands);
  const auto slot = Allocate(trace);
  if (slot == kAbsent) { return slot; }
  auto &sample = trace.Samples[slot];
  sample.Kind = 2;
  sample.Calls = 1;
  sample.Groups = groups;
  Identify(sample, pipeline);
  BeginQuery(trace, commands, slot, true);
  return slot;
}

inline void End(VkCommandBuffer commands, uint32_t slot) {
  const CpuScope cost;
  if (slot == kAbsent) { return; }
  auto &trace = *Traces.at(commands);
  EndQuery(trace, commands, slot, !trace.Samples[slot].Details);
}

inline void Finish(const Context &context,
                   VkCommandBuffer commands,
                   bool submitted,
                   const Result<SubmissionGpuTiming> &submission,
                   GpuTimingResults compute) {
  const auto found = Traces.find(commands);
  if (found == Traces.end()) { return; }
  if (!submitted) {
    ++CancelledCaptures;
    Traces.erase(found);
    return;
  }
  auto &trace = *found->second;
  trace.SubmissionNs = submission ? submission->ElapsedNanoseconds : -1;
  trace.Compute = std::move(compute);
  constexpr auto flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
  trace.StatisticsResult = vkGetQueryPoolResults(context.Logical,
                                                 trace.Statistics,
                                                 0,
                                                 trace.Count,
                                                 sizeof(trace.Counters),
                                                 trace.Counters.data(),
                                                 6 * sizeof(uint64_t),
                                                 flags);
  trace.TimestampResult = vkGetQueryPoolResults(context.Logical,
                                                trace.Times,
                                                0,
                                                2 * trace.Count,
                                                sizeof(trace.Timestamps),
                                                trace.Timestamps.data(),
                                                2 * sizeof(uint64_t),
                                                flags);
  trace.HostBoundNs = std::chrono::duration<double, std::nano>(Clock::now() - trace.Began).count();
  trace.Release();
  if (Completed.size() < kCaptureLimit) {
    Completed.push_back(std::move(found->second));
  } else {
    ++OmittedCaptures;
  }
  Traces.erase(found);
}

inline void Emit(const Trace &trace) {
  std::println(stderr,
               "VK_WORK_FRAME\t{}\t{}\t{}\t{}\t{}\t{}",
               trace.Serial,
               trace.Count,
               trace.Omitted,
               trace.SubmissionNs,
               trace.Compute.DispatchCount,
               trace.Compute.Truncated);
  const bool timestampsReadable =
      trace.TimestampResult == VK_SUCCESS || trace.TimestampResult == VK_NOT_READY;
  const bool statisticsReadable =
      trace.StatisticsResult == VK_SUCCESS || trace.StatisticsResult == VK_NOT_READY;
  for (uint32_t i = 0; i < trace.Count; ++i) {
    const auto &sample = trace.Samples[i];
    const auto *times = &trace.Timestamps[size_t{i} * 4];
    const auto *counts = &trace.Counters[size_t{i} * 6];
    const bool timeValid =
        timestampsReadable && times[1] != 0 && times[3] != 0 &&
        TimestampIntervalFits(times[0], times[2], trace.Bits, trace.Period, trace.HostBoundNs);
    const bool statsValid = !sample.Details && statisticsReadable && counts[5] != 0;
    std::println(stderr,
                 "VK_WORK_SAMPLE\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{"
                 "}\t{}\t{}\t{}\t{}\t{}",
                 trace.Serial,
                 i,
                 sample.Kind,
                 sample.Parent,
                 sample.Calls,
                 sample.Label,
                 sample.Missing,
                 timeValid,
                 statsValid,
                 sample.Fragment,
                 times[0],
                 times[2],
                 trace.Period,
                 trace.Bits,
                 counts[0],
                 counts[1],
                 counts[2],
                 counts[3],
                 counts[4],
                 sample.Groups.X,
                 sample.Groups.Y,
                 sample.Groups.Z);
  }
  for (const auto &dispatch : trace.Compute.Dispatches) {
    std::println(stderr,
                 "VK_WORK_COMPUTE\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
                 trace.Serial,
                 dispatch.Ordinal,
                 dispatch.Available,
                 dispatch.Begin,
                 dispatch.End,
                 dispatch.NanosecondsPerTick,
                 dispatch.ValidBits);
  }
}

inline void Clear(VkDevice device) {
  for (const auto &[command, trace] : Traces) {
    static_cast<void>(command);
    Require(trace->Device != device, "device destroyed with live query trace");
  }
  const auto began = Clock::now();
  for (const auto &trace : Completed) {
    if (trace->Device == device) { Emit(*trace); }
  }
  const auto formattingNs = std::chrono::duration<double, std::nano>(Clock::now() - began).count();
  std::println(stderr,
               "VK_WORK_COST\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
               CpuCalls,
               CpuNanoseconds,
               formattingNs,
               Completed.size(),
               OmittedCaptures,
               CancelledCaptures,
               sizeof(Trace));
  std::erase_if(Completed, [device](const auto &trace) { return trace->Device == device; });
  std::erase_if(Pipelines, [device](const auto &entry) { return entry.second.Device == device; });
}
}

#endif
