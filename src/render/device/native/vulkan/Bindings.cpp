#ifdef __linux__
#include "Recording.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace outshine::Render::Native::Vulkan {

Result<std::vector<VkDescriptorSet>> Recording::AllocateSets(const PipelineState &pipeline) {
  std::vector<VkDescriptorSet> sets(pipeline.Sets.size());
  if (sets.empty()) { return sets; }
  if (Descriptors_ == VK_NULL_HANDLE) {
    constexpr uint32_t descriptorSetBudget = 128;
    constexpr uint32_t storageDescriptorBudget = 256;
    constexpr uint32_t uniformDescriptorBudget = 64;
    constexpr uint32_t imageDescriptorBudget = 256;
    constexpr std::array sizes = {
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             .descriptorCount = storageDescriptorBudget},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                             .descriptorCount = uniformDescriptorBudget},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             .descriptorCount = imageDescriptorBudget},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                             .descriptorCount = imageDescriptorBudget}};
    VkDescriptorPoolCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    create.maxSets = descriptorSetBudget;
    create.poolSizeCount = static_cast<uint32_t>(sizes.size());
    create.pPoolSizes = sizes.data();
    const auto result = vkCreateDescriptorPool(Context_->Logical, &create, nullptr, &Descriptors_);
    if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateDescriptorPool", result)); }
  }
  VkDescriptorSetAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation.descriptorPool = Descriptors_;
  allocation.descriptorSetCount = static_cast<uint32_t>(sets.size());
  allocation.pSetLayouts = pipeline.Sets.data();
  const auto result = vkAllocateDescriptorSets(Context_->Logical, &allocation, sets.data());
  if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
    return std::unexpected(Error{.Kind = ErrorKind::Busy,
                                 .Message = "per-recording Vulkan descriptor budget exhausted"});
  }
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkAllocateDescriptorSets", result)); }
  return sets;
}

Result<VkDescriptorBufferInfo> Recording::BindBuffer(const ShaderBinding &declared,
                                                     const Binding &binding,
                                                     VkPipelineStageFlags2 stages) {
  std::shared_ptr<BufferState> data;
  ByteRange range;
  VkAccessFlags2 access = VK_ACCESS_2_SHADER_READ_BIT;
  if (declared.Kind == BindingKind::Uniform) {
    const auto *const uniform = std::get_if<Uniform>(&binding.Value);
    if (uniform == nullptr || uniform->Bytes.empty() ||
        uniform->Bytes.size() > Context_->Limits.MaxUniformBufferBytes ||
        uniform->Bytes.size() < declared.MinimumBytes) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "invalid shader uniform bytes"});
    }
    auto allocated = AllocateBuffer(
        Context_,
        {.Bytes = uniform->Bytes.size(), .Use = BufferUse::None, .Placement = Memory::Upload},
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    if (!allocated) { return std::unexpected(allocated.error()); }
    data = std::move(*allocated);
    range.Bytes = uniform->Bytes.size();
    const auto mapped = Vulkan::MapWrite(*data, range);
    if (!mapped) { return std::unexpected(mapped.error()); }
    std::memcpy(mapped->data(), uniform->Bytes.data(), uniform->Bytes.size());
    if (auto unmapped = Vulkan::Unmap(*data); !unmapped) {
      return std::unexpected(unmapped.error());
    }
    access = VK_ACCESS_2_UNIFORM_READ_BIT;
  } else {
    const auto *const storage = std::get_if<StorageBuffer>(&binding.Value);
    if (storage == nullptr || storage->Data == nullptr) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "missing shader storage buffer"});
    }
    const auto held = BufferFrom(*storage->Data, *Context_);
    if (!held) { return std::unexpected(held.error()); }
    data = *held;
    range = storage->Range;
    const bool storageUse = (static_cast<uint32_t>(data->Description.Use) &
                             static_cast<uint32_t>(BufferUse::Storage)) != 0;
    if (!storageUse || !Contains(data->Description.Bytes, range) ||
        range.Bytes < declared.MinimumBytes ||
        range.Bytes > Context_->Limits.MaxStorageBufferBytes ||
        range.Offset % Context_->Limits.StorageBufferAlignment != 0) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "invalid shader storage buffer range or alignment"});
    }
    if (declared.Usage != Access::Read) { access |= VK_ACCESS_2_SHADER_WRITE_BIT; }
    if (declared.Usage == Access::Write) { access = VK_ACCESS_2_SHADER_WRITE_BIT; }
  }
  if (auto held = Hold(data); !held) { return std::unexpected(held.error()); }
  Use(*data, range, {.Stage = stages, .Access = access});
  return VkDescriptorBufferInfo{
      .buffer = data->Handle, .offset = range.Offset, .range = range.Bytes};
}

Result<std::vector<VkDescriptorSet>> Recording::BindResources(const PipelineState &pipeline,
                                                              std::span<const ShaderBinding> layout,
                                                              std::span<const Binding> bindings,
                                                              VkPipelineStageFlags2 stages) {
  if (bindings.size() != layout.size()) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "shader binding count does not match reflection"});
  }
  auto sets = AllocateSets(pipeline);
  if (!sets) { return std::unexpected(sets.error()); }
  std::vector<VkDescriptorBufferInfo> buffers(layout.size());
  std::vector<VkDescriptorImageInfo> images(layout.size());
  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(layout.size());
  for (std::size_t at = 0; at < layout.size(); ++at) {
    const auto &declared = layout[at];
    const auto bound = std::ranges::find_if(bindings, [&declared](const Binding &binding) {
      return declared.Set == binding.Set && declared.Slot == binding.Slot;
    });
    if (bound == bindings.end()) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "shader reflected set/slot is not bound"});
    }
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = (*sets)[declared.Set];
    write.dstBinding = declared.Slot;
    write.descriptorCount = 1;
    write.descriptorType = DescriptorType(declared.Kind);
    if (declared.Kind == BindingKind::Uniform || declared.Kind == BindingKind::StorageBuffer) {
      const auto info = BindBuffer(declared, *bound, stages);
      if (!info) { return std::unexpected(info.error()); }
      buffers[at] = *info;
      write.pBufferInfo = &buffers[at];
    } else {
      const auto info = BindImage(declared, *bound, pipeline, stages);
      if (!info) { return std::unexpected(info.error()); }
      images[at] = *info;
      write.pImageInfo = &images[at];
    }
    writes.push_back(write);
  }
  vkUpdateDescriptorSets(
      Context_->Logical, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
  return sets;
}

}
#endif
