#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_IMAGE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_IMAGE_H

#ifdef __linux__
#include "Context.h"
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace outshine::Render::Native::Vulkan {

class Recording;

struct ImageState {
  std::shared_ptr<Context> Owner;
  VkImage Handle = VK_NULL_HANDLE;
  VkDeviceMemory Allocation = VK_NULL_HANDLE;
  TextureDescription Description;
  std::vector<VkImageLayout> Layouts;
  Recording *Reserved = nullptr;

  ImageState() = default;
  ~ImageState();
  ImageState(const ImageState &) = delete;
  ImageState &operator=(const ImageState &) = delete;
};

class VulkanTexture final : public Texture {
public:
  explicit VulkanTexture(std::shared_ptr<ImageState> state)
      : Texture(state->Description), State_(std::move(state)) {}

  [[nodiscard]] const std::shared_ptr<ImageState> &State() const noexcept { return State_; }

private:
  std::shared_ptr<ImageState> State_;
};

struct SamplerState {
  std::shared_ptr<Context> Owner;
  VkSampler Handle = VK_NULL_HANDLE;
  bool Filtered = false;

  SamplerState() = default;
  ~SamplerState();
  SamplerState(const SamplerState &) = delete;
  SamplerState &operator=(const SamplerState &) = delete;
};

class VulkanSampler final : public Sampler {
public:
  explicit VulkanSampler(std::shared_ptr<SamplerState> state) : State_(std::move(state)) {}

  [[nodiscard]] const std::shared_ptr<SamplerState> &State() const noexcept { return State_; }

private:
  std::shared_ptr<SamplerState> State_;
};

[[nodiscard]] VkFormat TexelFormat(Format format) noexcept;
[[nodiscard]] uint32_t TexelBytes(Format format) noexcept;
[[nodiscard]] VkImageAspectFlags Aspect(Format format) noexcept;
[[nodiscard]] VkImageUsageFlags ImageUsage(UseMask<TextureUse> use) noexcept;
[[nodiscard]] bool
Supports(const Context &context, Format format, UseMask<TextureUse> use, bool filtered);
[[nodiscard]] Result<std::shared_ptr<ImageState>> AllocateImage(std::shared_ptr<Context> context,
                                                                TextureDescription description);
[[nodiscard]] Result<std::shared_ptr<SamplerState>>
AllocateSampler(std::shared_ptr<Context> context, SamplerDescription description);
[[nodiscard]] Result<std::shared_ptr<ImageState>> ImageFrom(const Texture &image,
                                                            const Context &context);
[[nodiscard]] bool ValidView(const ImageState &image, TextureView view) noexcept;
[[nodiscard]] Result<VkImageView> CreateView(const ImageState &image, TextureView view);
[[nodiscard]] bool ValidRegion(const ImageState &image, TextureRegion region) noexcept;

}
#endif
#endif
