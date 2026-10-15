#ifdef __linux__
#include "Recording.h"
#include <expected>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>

namespace outshine::Render::Native::Vulkan {

Recording::~Recording() {
  assert(OnOwner(*Context_));
  if (Pool_ != VK_NULL_HANDLE) { vkDestroyCommandPool(Context_->Logical, Pool_, nullptr); }
  if (Descriptors_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(Context_->Logical, Descriptors_, nullptr);
  }
  for (auto *const view : Views_) { vkDestroyImageView(Context_->Logical, view, nullptr); }
  for (const auto &tracked : Images_) {
    if (tracked.Image->Reserved == this) { tracked.Image->Reserved = nullptr; }
  }
  for (const auto &buffer : Buffers_) { --buffer->RecordedUses; }
  --Context_->Recordings;
}

Result<void> Recording::Initialize() {
  VkCommandPoolCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  create.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  create.queueFamilyIndex = Context_->QueueFamily;
  auto result = vkCreateCommandPool(Context_->Logical, &create, nullptr, &Pool_);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateCommandPool", result)); }
  VkCommandBufferAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocation.commandPool = Pool_;
  allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocation.commandBufferCount = 1;
  result = vkAllocateCommandBuffers(Context_->Logical, &allocation, &Commands_);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkAllocateCommandBuffers", result)); }
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  result = vkBeginCommandBuffer(Commands_, &begin);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkBeginCommandBuffer", result)); }
  return {};
}

Result<void> Recording::End() {
  if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
  if (Finished_) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid, .Message = "commands already ended"});
  }
  const auto result = vkEndCommandBuffer(Commands_);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkEndCommandBuffer", result)); }
  Finished_ = true;
  return {};
}

Result<void> Recording::Hold(const std::shared_ptr<BufferState> &buffer) {
  if (buffer->Mapped != nullptr || Finished_) {
    return std::unexpected(Error{.Kind = ErrorKind::Busy,
                                 .Message = "buffer is mapped or command recording has ended"});
  }
  if (std::ranges::find(Buffers_, buffer) == Buffers_.end()) {
    Buffers_.push_back(buffer);
    ++buffer->RecordedUses;
  }
  return {};
}

void Recording::Use(const BufferState &buffer, ByteRange range, BufferAccess access) {
  VkBufferMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
  barrier.dstStageMask = access.Stage;
  barrier.dstAccessMask = access.Access;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.buffer = buffer.Handle;
  barrier.offset = range.Offset;
  barrier.size = range.Bytes;
  VkDependencyInfo dependency{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.bufferMemoryBarrierCount = 1;
  dependency.pBufferMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(Commands_, &dependency);
}

Result<void> Recording::Copy(const Buffer &source, Buffer &destination, BufferCopy region) {
  if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
  const auto from = BufferFrom(source, *Context_);
  const auto into = BufferFrom(destination, *Context_);
  if (!from) { return std::unexpected(from.error()); }
  if (!into) { return std::unexpected(into.error()); }
  const auto has = [](UseMask<BufferUse> use, BufferUse bit) {
    return (static_cast<uint32_t>(use) & static_cast<uint32_t>(bit)) != 0;
  };
  const ByteRange sourceRange{.Offset = region.SourceOffset, .Bytes = region.Bytes};
  const ByteRange destinationRange{.Offset = region.DestinationOffset, .Bytes = region.Bytes};
  if (!Contains(source.Description().Bytes, sourceRange) ||
      !Contains(destination.Description().Bytes, destinationRange) ||
      !has(source.Description().Use, BufferUse::CopySource) ||
      !has(destination.Description().Use, BufferUse::CopyDestination) ||
      (from->get() == into->get())) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid, .Message = "invalid buffer copy"});
  }
  if (auto held = Hold(*from); !held) { return std::unexpected(held.error()); }
  if (auto held = Hold(*into); !held) { return std::unexpected(held.error()); }
  Use(**from,
      sourceRange,
      {.Stage = VK_PIPELINE_STAGE_2_COPY_BIT, .Access = VK_ACCESS_2_TRANSFER_READ_BIT});
  Use(**into,
      destinationRange,
      {.Stage = VK_PIPELINE_STAGE_2_COPY_BIT, .Access = VK_ACCESS_2_TRANSFER_WRITE_BIT});
  const VkBufferCopy copy{.srcOffset = region.SourceOffset,
                          .dstOffset = region.DestinationOffset,
                          .size = region.Bytes};
  vkCmdCopyBuffer(Commands_, (*from)->Handle, (*into)->Handle, 1, &copy);
  if (destination.Description().Placement == Memory::Readback) {
    Use(**into,
        destinationRange,
        {.Stage = VK_PIPELINE_STAGE_2_HOST_BIT, .Access = VK_ACCESS_2_HOST_READ_BIT});
  }
  return {};
}

void Recording::BeginLabel(std::string_view name) {
  if (!Context_->DebugMarkers) { return; }
  const auto begin = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
      vkGetDeviceProcAddr(Context_->Logical, "vkCmdBeginDebugUtilsLabelEXT"));
  if (begin == nullptr || Finished_) { return; }
  const std::string text(name);
  VkDebugUtilsLabelEXT label{};
  label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  label.pLabelName = text.c_str();
  begin(Commands_, &label);
}

void Recording::EndLabel() {
  if (!Context_->DebugMarkers) { return; }
  const auto end = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
      vkGetDeviceProcAddr(Context_->Logical, "vkCmdEndDebugUtilsLabelEXT"));
  if (end != nullptr && !Finished_) { end(Commands_); }
}

}
#endif
