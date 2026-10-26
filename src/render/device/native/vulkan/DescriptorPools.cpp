#ifdef __linux__
#include "BindingStorage.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Render::Native::Vulkan {
Result<void> BindingStorage::NewPool(std::span<const DescriptorResource> resources) {
  if (Pools_.size() == kPoolsMost) {
    return std::unexpected(Error{.Kind = ErrorKind::Busy,
                                 .Message = "Vulkan recording descriptor-pool capacity exhausted"});
  }
  constexpr uint32_t setBudget = 128;
  constexpr uint32_t storageBudget = 256;
  constexpr uint32_t uniformBudget = 64;
  constexpr uint32_t imageBudget = 256;
  std::array sizes{VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        .descriptorCount = storageBudget},
                   VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        .descriptorCount = uniformBudget},
                   VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        .descriptorCount = imageBudget},
                   VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                        .descriptorCount = imageBudget}};
  for (auto &size : sizes) {
    const auto needed = std::ranges::count(resources, size.type, &DescriptorResource::Type);
    size.descriptorCount = std::max(size.descriptorCount, static_cast<uint32_t>(needed));
  }
  VkDescriptorPoolCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  create.maxSets = setBudget;
  create.poolSizeCount = static_cast<uint32_t>(sizes.size());
  create.pPoolSizes = sizes.data();
  VkDescriptorPool pool = VK_NULL_HANDLE;
  const auto made = vkCreateDescriptorPool(Context_->Logical, &create, nullptr, &pool);
  if (made != VK_SUCCESS) { return std::unexpected(Failure("vkCreateDescriptorPool", made)); }
  Pools_.push_back(pool);
  ++Work_.Pools;
  return {};
}

Result<VkDescriptorSet> BindingStorage::AllocateSet(VkDescriptorSetLayout layout,
                                                    std::span<const DescriptorResource> resources) {
  if (Pools_.empty()) {
    const auto made = NewPool(resources);
    if (!made) { return std::unexpected(made.error()); }
  }
  VkDescriptorSetAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation.descriptorSetCount = 1;
  allocation.pSetLayouts = &layout;
  VkDescriptorSet set = VK_NULL_HANDLE;
  allocation.descriptorPool = Pools_.back();
  auto result = vkAllocateDescriptorSets(Context_->Logical, &allocation, &set);
  if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
    const auto made = NewPool(resources);
    if (!made) { return std::unexpected(made.error()); }
    allocation.descriptorPool = Pools_.back();
    result = vkAllocateDescriptorSets(Context_->Logical, &allocation, &set);
  }
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkAllocateDescriptorSets", result)); }
  return set;
}

void BindingStorage::WriteSet(VkDescriptorSet set, std::span<const DescriptorResource> resources) {
  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(resources.size());
  for (const auto &resource : resources) {
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = resource.Slot;
    write.descriptorCount = 1;
    write.descriptorType = resource.Type;
    if (resource.Type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
        resource.Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
      write.pBufferInfo = &resource.Buffer;
    } else {
      write.pImageInfo = &resource.Image;
    }
    writes.push_back(write);
  }
  vkUpdateDescriptorSets(
      Context_->Logical, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
}
#endif
