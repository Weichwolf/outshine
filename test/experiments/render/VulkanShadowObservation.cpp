#include <vulkan/vulkan.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <mutex>
#include <print>

namespace {
struct Commands {
  VkCommandBuffer Handle{};
  uint32_t Viewports{}, ShadowViewports{}, Mask{};
  bool Ended{};
};

struct Submission {
  uint64_t Serial{};
  uint32_t Queue{}, Viewports{}, ShadowViewports{}, Mask{};
  bool Timed{}, Ended{};
};

struct Queue {
  VkQueue Handle{};
  uint64_t Serial{};
};

std::mutex Mutex;
std::array<Commands, 4096> Records;
std::array<Submission, 4096> Submissions;
std::array<Queue, 4> Queues;
size_t RecordCount{}, SubmissionCount{}, QueueCount{};
uint64_t Dropped{}, TimestampCalls{}, ViewportCalls{}, SubmitCalls{};
Commands *Cached{};

template <class T> T Next(const char *name) {
  auto *const address = dlsym(RTLD_NEXT, name);
  if (address == nullptr) { std::abort(); }
  return reinterpret_cast<T>(address);
}

Commands *Find(VkCommandBuffer handle, bool create) {
  if (Cached != nullptr && Cached->Handle == handle) { return Cached; }
  for (size_t i = 0; i < RecordCount; ++i) {
    if (Records[i].Handle == handle) { return Cached = &Records[i]; }
  }
  if (create && RecordCount < Records.size()) {
    Cached = &Records[RecordCount++];
    Cached->Handle = handle;
    return Cached;
  }
  ++Dropped;
  return nullptr;
}

struct Dump {
  ~Dump() {
    FILE *const out = stderr;
    std::fputc('\n', out);
    std::print(out,
               "{{\"dropped\":{},\"records\":{},\"queues\":{},\"timestamp_calls\":{},"
               "\"viewport_calls\":{},\"submit_calls\":{},\"table_bytes\":{},\"submissions\":[",
               Dropped,
               RecordCount,
               QueueCount,
               TimestampCalls,
               ViewportCalls,
               SubmitCalls,
               sizeof(Records) + sizeof(Submissions) + sizeof(Queues));
    for (size_t i = 0; i < SubmissionCount; ++i) {
      const auto &s = Submissions[i];
      std::print(out,
                 "{}{{\"serial\":{},\"queue\":{},\"viewports\":{},\"shadow_viewports\":{},"
                 "\"mask\":{},\"timed\":{},\"ended\":{}}}",
                 i != 0 ? "," : "",
                 s.Serial,
                 s.Queue,
                 s.Viewports,
                 s.ShadowViewports,
                 s.Mask,
                 s.Timed,
                 s.Ended);
    }
    std::println(out, "]}}");
  }
} DumpAtExit;
}

extern "C" {
VKAPI_ATTR void VKAPI_CALL vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer,
                                                VkPipelineStageFlags2 stage,
                                                VkQueryPool queryPool,
                                                uint32_t query) {
  static const auto next = Next<PFN_vkCmdWriteTimestamp2>("vkCmdWriteTimestamp2");
  next(commandBuffer, stage, queryPool, query);
  const std::scoped_lock lock(Mutex);
  ++TimestampCalls;
  if (query == 0) {
    if (auto *r = Find(commandBuffer, true); r != nullptr) {
      *r = Commands{.Handle = commandBuffer};
    }
  } else if (query == 1) {
    if (auto *r = Find(commandBuffer, false); r != nullptr) { r->Ended = true; }
  }
}

VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(VkCommandBuffer commandBuffer,
                                            uint32_t firstViewport,
                                            uint32_t viewportCount,
                                            const VkViewport *pViewports) {
  static const auto next = Next<PFN_vkCmdSetViewport>("vkCmdSetViewport");
  next(commandBuffer, firstViewport, viewportCount, pViewports);
  const std::scoped_lock lock(Mutex);
  ++ViewportCalls;
  auto *r = Find(commandBuffer, false);
  if (r == nullptr) { return; }
  r->Viewports += viewportCount;
  for (uint32_t i = 0; i < viewportCount; ++i) {
    const auto &a = pViewports[i];
    if (a.width != 2048 || (a.height != 2048 && a.height != -2048)) { continue; }
    const auto y = a.height < 0 ? a.y + a.height : a.y;
    if ((a.x != 0 && a.x != 2048) || (y != 0 && y != 2048)) {
      ++Dropped;
      continue;
    }
    ++r->ShadowViewports;
    const auto tile = static_cast<uint32_t>(a.x / 2048) + 2 * static_cast<uint32_t>(y / 2048);
    r->Mask |= 1U << tile;
  }
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue,
                                              uint32_t submitCount,
                                              const VkSubmitInfo2 *pSubmits,
                                              VkFence fence) {
  static const auto next = Next<PFN_vkQueueSubmit2>("vkQueueSubmit2");
  const auto result = next(queue, submitCount, pSubmits, fence);
  const std::scoped_lock lock(Mutex);
  ++SubmitCalls;
  if (result != VK_SUCCESS) {
    ++Dropped;
    return result;
  }
  size_t q = 0;
  while (q < QueueCount && Queues[q].Handle != queue) { ++q; }
  if (q == QueueCount) {
    if (q == Queues.size()) {
      ++Dropped;
      return result;
    }
    Queues[QueueCount++].Handle = queue;
  }
  const auto serial = ++Queues[q].Serial;
  if (submitCount != 1 || pSubmits[0].commandBufferInfoCount != 1 ||
      SubmissionCount == Submissions.size()) {
    ++Dropped;
    return result;
  }
  const auto *r = Find(pSubmits[0].pCommandBufferInfos[0].commandBuffer, false);
  auto &s = Submissions[SubmissionCount++];
  s.Serial = serial;
  s.Queue = static_cast<uint32_t>(q + 1);
  s.Timed = (r != nullptr);
  if (r != nullptr) {
    s.Ended = r->Ended;
    s.Viewports = r->Viewports;
    s.ShadowViewports = r->ShadowViewports;
    s.Mask = r->Mask;
  }
  return result;
}
}
