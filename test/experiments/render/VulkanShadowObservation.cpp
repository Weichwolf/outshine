#include <vulkan/vulkan.h>
#include "VulkanShadowWork.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <mutex>
#include <optional>
#include <print>

namespace {
using namespace outshine::Experiment;

struct Commands {
  VkCommandBuffer Handle{};
  uint32_t Viewports{}, ShadowViewports{}, Mask{};
  bool Ended{};
  int32_t ShadowRegion = -1;
  VkRect2D Clip{};
  bool ClipKnown{}, PipelineKnown{}, Fragment{};
  uint32_t VertexBindings{}, LastPatch = kNoShadowPatch;
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

struct Pipeline {
  VkPipeline Handle{};
  uint32_t VertexBindings{};
  bool Known{}, Fragment{};
};

std::mutex Mutex;
std::array<Commands, 4096> Records;
std::array<Submission, 4096> Submissions;
std::array<Queue, 4> Queues;
std::array<Pipeline, 4096> Pipelines;
ShadowWorkTable Work;
size_t RecordCount{}, SubmissionCount{}, QueueCount{};
size_t PipelineCount{};
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

Pipeline *FindPipeline(VkPipeline handle, bool create) {
  for (size_t at = 0; at < PipelineCount; ++at) {
    if (Pipelines[at].Handle == handle) { return &Pipelines[at]; }
  }
  if (create && PipelineCount < Pipelines.size()) {
    auto &pipeline = Pipelines[PipelineCount++];
    pipeline.Handle = handle;
    return &pipeline;
  }
  if (create) { ++Dropped; }
  return nullptr;
}

std::optional<ShadowPatchAddress> PatchOf(Commands *command) {
  if (command == nullptr || command->ShadowRegion < 0) { return std::nullopt; }
  if (!command->ClipKnown) {
    ++Dropped;
    return std::nullopt;
  }
  return ShadowPatchAddress{.Region = static_cast<uint32_t>(command->ShadowRegion),
                            .X = command->Clip.offset.x,
                            .Y = command->Clip.offset.y,
                            .Width = command->Clip.extent.width,
                            .Height = command->Clip.extent.height};
}

void DumpWork(FILE *out) {
  std::print(out, ",\"shadow_patches\":[");
  size_t at = 0;
  for (const auto &row : Work.Rows()) {
    std::print(out,
               "{}{{\"serial\":{},\"queue\":{},\"region\":{},\"x\":{},\"y\":{},"
               "\"width\":{},\"height\":{},\"indexed_packets\":{},\"index_requests\":{},"
               "\"indexed_instances\":{},\"instance_index_requests\":{},"
               "\"one_stream_requests\":{},\"two_stream_requests\":{},"
               "\"other_stream_requests\":{},\"unknown_pipeline_packets\":{},"
               "\"fragment_requests\":{},\"nonindexed_packets\":{},"
               "\"instance_vertex_requests\":{},\"indirect_packets\":{},"
               "\"indirect_records\":{},\"largest\":[",
               at++ != 0 ? "," : "",
               row.Serial,
               row.Queue,
               row.Address.Region,
               row.Address.X,
               row.Address.Y,
               row.Address.Width,
               row.Address.Height,
               row.IndexedPackets,
               row.IndexRequests,
               row.IndexedInstances,
               row.InstanceIndexRequests,
               row.OneStreamRequests,
               row.TwoStreamRequests,
               row.OtherStreamRequests,
               row.UnknownPipelinePackets,
               row.FragmentRequests,
               row.NonIndexedPackets,
               row.InstanceVertexRequests,
               row.IndirectPackets,
               row.IndirectRecords);
    size_t item = 0;
    for (const auto &packet : row.Largest) {
      if (packet.IndexCount == 0) { continue; }
      std::print(out,
                 "{}{{\"first_index\":{},\"index_count\":{},\"instances\":{},"
                 "\"first_instance\":{},\"vertex_offset\":{},\"vertex_bindings\":{},"
                 "\"pipeline_known\":{},\"fragment\":{}}}",
                 item++ != 0 ? "," : "",
                 packet.FirstIndex,
                 packet.IndexCount,
                 packet.Instances,
                 packet.FirstInstance,
                 packet.VertexOffset,
                 packet.VertexBindings,
                 packet.PipelineKnown,
                 packet.Fragment);
    }
    std::print(out, "]}}");
  }
  std::print(out, "]");
}

struct Dump {
  ~Dump() {
    FILE *const out = stderr;
    std::fputc('\n', out);
    std::print(out,
               "{{\"dropped\":{},\"records\":{},\"queues\":{},\"timestamp_calls\":{},"
               "\"viewport_calls\":{},\"submit_calls\":{},\"table_bytes\":{},"
               "\"pipeline_records\":{},\"submissions\":[",
               Dropped + Work.Dropped(),
               RecordCount,
               QueueCount,
               TimestampCalls,
               ViewportCalls,
               SubmitCalls,
               sizeof(Records) + sizeof(Submissions) + sizeof(Queues) + sizeof(Pipelines) +
                   sizeof(Work),
               PipelineCount);
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
    std::print(out, "]");
    DumpWork(out);
    std::println(out, "}}");
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
  r->ShadowRegion = -1;
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
    if (firstViewport == 0 && viewportCount == 1) {
      r->ShadowRegion = static_cast<int32_t>(tile);
    } else {
      ++Dropped;
    }
  }
}

VKAPI_ATTR VkResult VKAPI_CALL
vkCreateGraphicsPipelines(VkDevice device,
                          VkPipelineCache pipelineCache,
                          uint32_t createInfoCount,
                          const VkGraphicsPipelineCreateInfo *pCreateInfos,
                          const VkAllocationCallbacks *pAllocator,
                          VkPipeline *pPipelines) {
  static const auto next = Next<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines");
  const auto result =
      next(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
  const std::scoped_lock lock(Mutex);
  for (uint32_t at = 0; at < createInfoCount; ++at) {
    if (pPipelines[at] == VK_NULL_HANDLE) { continue; }
    auto *pipeline = FindPipeline(pPipelines[at], true);
    if (pipeline == nullptr) { continue; }
    const auto &description = pCreateInfos[at];
    pipeline->Known = description.pVertexInputState != nullptr;
    pipeline->VertexBindings =
        pipeline->Known ? description.pVertexInputState->vertexBindingDescriptionCount : 0;
    pipeline->Fragment = false;
    for (uint32_t stage = 0; stage < description.stageCount; ++stage) {
      pipeline->Fragment |= (description.pStages[stage].stage & VK_SHADER_STAGE_FRAGMENT_BIT) != 0;
    }
  }
  return result;
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer,
                                             VkPipelineBindPoint pipelineBindPoint,
                                             VkPipeline pipeline) {
  static const auto next = Next<PFN_vkCmdBindPipeline>("vkCmdBindPipeline");
  next(commandBuffer, pipelineBindPoint, pipeline);
  if (pipelineBindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) { return; }
  const std::scoped_lock lock(Mutex);
  auto *command = Find(commandBuffer, false);
  if (command == nullptr) { return; }
  const auto *found = FindPipeline(pipeline, false);
  command->PipelineKnown = found != nullptr && found->Known;
  command->VertexBindings = found != nullptr ? found->VertexBindings : 0;
  command->Fragment = found != nullptr && found->Fragment;
}

VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(VkCommandBuffer commandBuffer,
                                           uint32_t firstScissor,
                                           uint32_t scissorCount,
                                           const VkRect2D *pScissors) {
  static const auto next = Next<PFN_vkCmdSetScissor>("vkCmdSetScissor");
  next(commandBuffer, firstScissor, scissorCount, pScissors);
  const std::scoped_lock lock(Mutex);
  auto *command = Find(commandBuffer, false);
  if (command == nullptr) { return; }
  command->ClipKnown = firstScissor == 0 && scissorCount == 1;
  if (command->ClipKnown) { command->Clip = pScissors[0]; }
}

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer commandBuffer,
                                            uint32_t indexCount,
                                            uint32_t instanceCount,
                                            uint32_t firstIndex,
                                            int32_t vertexOffset,
                                            uint32_t firstInstance) {
  static const auto next = Next<PFN_vkCmdDrawIndexed>("vkCmdDrawIndexed");
  next(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
  const std::scoped_lock lock(Mutex);
  auto *command = Find(commandBuffer, false);
  const auto patch = PatchOf(command);
  if (!patch) { return; }
  Work.Indexed(command->LastPatch,
               *patch,
               {.FirstIndex = firstIndex,
                .IndexCount = indexCount,
                .Instances = instanceCount,
                .FirstInstance = firstInstance,
                .VertexOffset = vertexOffset,
                .VertexBindings = command->VertexBindings,
                .PipelineKnown = command->PipelineKnown,
                .Fragment = command->Fragment});
}

VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer commandBuffer,
                                     uint32_t vertexCount,
                                     uint32_t instanceCount,
                                     uint32_t firstVertex,
                                     uint32_t firstInstance) {
  static const auto next = Next<PFN_vkCmdDraw>("vkCmdDraw");
  next(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
  const std::scoped_lock lock(Mutex);
  auto *command = Find(commandBuffer, false);
  const auto patch = PatchOf(command);
  if (patch) { Work.NonIndexed(command->LastPatch, *patch, vertexCount, instanceCount); }
}

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer,
                                                    VkBuffer buffer,
                                                    VkDeviceSize offset,
                                                    uint32_t drawCount,
                                                    uint32_t stride) {
  static const auto next = Next<PFN_vkCmdDrawIndexedIndirect>("vkCmdDrawIndexedIndirect");
  next(commandBuffer, buffer, offset, drawCount, stride);
  const std::scoped_lock lock(Mutex);
  auto *command = Find(commandBuffer, false);
  const auto patch = PatchOf(command);
  if (patch) { Work.Indirect(command->LastPatch, *patch, drawCount); }
}

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirect(VkCommandBuffer commandBuffer,
                                             VkBuffer buffer,
                                             VkDeviceSize offset,
                                             uint32_t drawCount,
                                             uint32_t stride) {
  static const auto next = Next<PFN_vkCmdDrawIndirect>("vkCmdDrawIndirect");
  next(commandBuffer, buffer, offset, drawCount, stride);
  const std::scoped_lock lock(Mutex);
  auto *command = Find(commandBuffer, false);
  const auto patch = PatchOf(command);
  if (patch) { Work.Indirect(command->LastPatch, *patch, drawCount); }
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
    Work.Submitted(r->LastPatch, {.Serial = serial, .Queue = s.Queue});
  }
  return result;
}
}
