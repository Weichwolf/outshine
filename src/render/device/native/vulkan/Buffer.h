#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_BUFFER_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_BUFFER_H

#ifdef __linux__
#include "Context.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace outshine::Render::Native::Vulkan {

struct BufferState {
  std::shared_ptr<Context> Owner;
  VkBuffer Handle = VK_NULL_HANDLE;
  VkDeviceMemory Allocation = VK_NULL_HANDLE;
  VkMemoryPropertyFlags Properties = 0;
  BufferDescription Description;
  void *Mapped = nullptr;
  bool Writing = false;
  uint32_t RecordedUses = 0;

  BufferState() = default;
  ~BufferState();
  BufferState(const BufferState &) = delete;
  BufferState &operator=(const BufferState &) = delete;
};

class VulkanBuffer final : public Buffer {
public:
  explicit VulkanBuffer(std::shared_ptr<BufferState> state)
      : Buffer(state->Description), State_(std::move(state)) {}

  [[nodiscard]] const std::shared_ptr<BufferState> &State() const noexcept { return State_; }

private:
  std::shared_ptr<BufferState> State_;
};

[[nodiscard]] bool Contains(uint64_t capacity, ByteRange range) noexcept;
[[nodiscard]] Result<std::shared_ptr<BufferState>>
AllocateBuffer(std::shared_ptr<Context> context,
               BufferDescription description,
               VkBufferUsageFlags additionalUse = 0);
[[nodiscard]] Result<std::shared_ptr<BufferState>> BufferFrom(const Buffer &buffer,
                                                              const Context &context);
[[nodiscard]] Result<std::span<std::byte>> MapWrite(BufferState &state, ByteRange range);
[[nodiscard]] Result<std::span<const std::byte>> MapRead(BufferState &state, ByteRange range);
[[nodiscard]] Result<void> Unmap(BufferState &state);

}
#endif
#endif
