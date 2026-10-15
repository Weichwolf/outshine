#ifdef __linux__
#include "Buffer.h"
#include <expected>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <utility>

namespace outshine::Render::Native::Vulkan {

BufferState::~BufferState() {
  if (Mapped != nullptr) { vkUnmapMemory(Owner->Logical, Allocation); }
  if (Handle != VK_NULL_HANDLE) { vkDestroyBuffer(Owner->Logical, Handle, nullptr); }
  if (Allocation != VK_NULL_HANDLE) { vkFreeMemory(Owner->Logical, Allocation, nullptr); }
}

bool Contains(uint64_t capacity, ByteRange range) noexcept {
  return range.Bytes > 0 && range.Offset <= capacity && range.Bytes <= capacity - range.Offset;
}

namespace {

VkBufferUsageFlags BufferUsage(UseMask<BufferUse> use) {
  const auto has = [use](BufferUse bit) {
    return (static_cast<uint32_t>(use) & static_cast<uint32_t>(bit)) != 0;
  };
  VkBufferUsageFlags flags = 0;
  if (has(BufferUse::CopySource)) { flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT; }
  if (has(BufferUse::CopyDestination)) { flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; }
  if (has(BufferUse::Storage)) { flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; }
  if (has(BufferUse::Vertex)) { flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; }
  if (has(BufferUse::Index)) { flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT; }
  if (has(BufferUse::Indirect)) { flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT; }
  return flags;
}

Result<void> BindMemory(BufferState &state) {
  const auto &context = *state.Owner;
  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(context.Logical, state.Handle, &requirements);
  const auto required = state.Description.Placement == Memory::Device
                            ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                            : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  const auto preferred = state.Description.Placement == Memory::Readback
                             ? VK_MEMORY_PROPERTY_HOST_CACHED_BIT
                             : VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  const auto type = FindMemory(
      context, requirements.memoryTypeBits, {.Required = required, .Preferred = preferred});
  if (!type) { return std::unexpected(type.error()); }
  state.Properties = context.MemoryProperties.memoryTypes[*type].propertyFlags;
  VkMemoryAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocation.allocationSize = requirements.size;
  allocation.memoryTypeIndex = *type;
  auto result = vkAllocateMemory(context.Logical, &allocation, nullptr, &state.Allocation);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkAllocateMemory", result)); }
  result = vkBindBufferMemory(context.Logical, state.Handle, state.Allocation, 0);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkBindBufferMemory", result)); }
  return {};
}

Result<void *> Map(BufferState &state, ByteRange range, Memory placement) {
  if (state.Description.Placement != placement || !Contains(state.Description.Bytes, range) ||
      range.Bytes > std::numeric_limits<std::size_t>::max()) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid, .Message = "invalid Vulkan host map"});
  }
  if (state.Mapped != nullptr || state.RecordedUses != 0) {
    return std::unexpected(Error{.Kind = ErrorKind::Busy,
                                 .Message = "Vulkan buffer is mapped, recorded or in flight"});
  }
  const auto result =
      vkMapMemory(state.Owner->Logical, state.Allocation, 0, VK_WHOLE_SIZE, 0, &state.Mapped);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkMapMemory", result)); }
  state.Writing = placement == Memory::Upload;
  return static_cast<std::byte *>(state.Mapped) + range.Offset;
}

}

Result<std::shared_ptr<BufferState>> AllocateBuffer(std::shared_ptr<Context> context,
                                                    BufferDescription description,
                                                    VkBufferUsageFlags additionalUse) {
  constexpr auto known = BufferUse::CopySource | BufferUse::CopyDestination | BufferUse::Storage |
                         BufferUse::Vertex | BufferUse::Index | BufferUse::Indirect;
  if ((static_cast<uint32_t>(description.Use) & ~static_cast<uint32_t>(known)) != 0 ||
      (description.Placement != Memory::Device && description.Placement != Memory::Upload &&
       description.Placement != Memory::Readback)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid buffer use or memory"});
  }
  const auto usage = BufferUsage(description.Use) | additionalUse;
  if (description.Bytes == 0 || usage == 0) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "Vulkan buffer needs nonzero size and use"});
  }
  auto state = std::make_shared<BufferState>();
  state->Owner = std::move(context);
  state->Description = description;
  VkBufferCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create.size = description.Bytes;
  create.usage = usage;
  create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  const auto result = vkCreateBuffer(state->Owner->Logical, &create, nullptr, &state->Handle);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateBuffer", result)); }
  if (auto bound = BindMemory(*state); !bound) { return std::unexpected(bound.error()); }
  return state;
}

Result<std::shared_ptr<BufferState>> BufferFrom(const Buffer &buffer, const Context &context) {
  const auto *const native = dynamic_cast<const VulkanBuffer *>(&buffer);
  if (native == nullptr || native->State()->Owner.get() != &context) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "buffer belongs to a different Vulkan device"});
  }
  return native->State();
}

Result<std::span<std::byte>> MapWrite(BufferState &state, ByteRange range) {
  const auto mapped = Map(state, range, Memory::Upload);
  if (!mapped) { return std::unexpected(mapped.error()); }
  return std::span(static_cast<std::byte *>(*mapped), static_cast<std::size_t>(range.Bytes));
}

Result<std::span<const std::byte>> MapRead(BufferState &state, ByteRange range) {
  const auto mapped = Map(state, range, Memory::Readback);
  if (!mapped) { return std::unexpected(mapped.error()); }
  if ((state.Properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
    VkMappedMemoryRange visible{};
    visible.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    visible.memory = state.Allocation;
    visible.size = VK_WHOLE_SIZE;
    const auto result = vkInvalidateMappedMemoryRanges(state.Owner->Logical, 1, &visible);
    if (result != VK_SUCCESS) {
      vkUnmapMemory(state.Owner->Logical, state.Allocation);
      state.Mapped = nullptr;
      return std::unexpected(Failure("vkInvalidateMappedMemoryRanges", result));
    }
  }
  return std::span(static_cast<const std::byte *>(*mapped), static_cast<std::size_t>(range.Bytes));
}

Result<void> Unmap(BufferState &state) {
  if (state.Mapped == nullptr) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "Vulkan buffer is not mapped"});
  }
  VkResult result = VK_SUCCESS;
  if (state.Writing && (state.Properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
    VkMappedMemoryRange visible{};
    visible.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    visible.memory = state.Allocation;
    visible.size = VK_WHOLE_SIZE;
    result = vkFlushMappedMemoryRanges(state.Owner->Logical, 1, &visible);
  }
  vkUnmapMemory(state.Owner->Logical, state.Allocation);
  state.Mapped = nullptr;
  state.Writing = false;
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkFlushMappedMemoryRanges", result));
  }
  return {};
}

}
#endif
