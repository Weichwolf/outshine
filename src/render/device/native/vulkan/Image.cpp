#ifdef __linux__
#include "VulkanImage.h"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
#include <utility>

namespace outshine::Render::Native::Vulkan {

ImageState::~ImageState() {
  if (Handle != VK_NULL_HANDLE) { vkDestroyImage(Owner->Logical, Handle, nullptr); }
  if (Allocation != VK_NULL_HANDLE) { vkFreeMemory(Owner->Logical, Allocation, nullptr); }
}

namespace {

Result<void> CheckImage(const Context &context, TextureDescription description) {
  if (TexelFormat(description.Texels) == VK_FORMAT_UNDEFINED || description.Width == 0 ||
      description.Height == 0 || description.Mips == 0 || description.Layers == 0 ||
      std::cmp_greater(description.Mips,
                       std::bit_width(std::max(description.Width, description.Height))) ||
      description.Width > context.Limits.MaxTextureExtent ||
      description.Height > context.Limits.MaxTextureExtent ||
      description.Layers > context.Limits.MaxTextureLayers ||
      (description.Type != TextureType::Image2D && description.Type != TextureType::Image2DArray) ||
      (description.Type == TextureType::Image2D && description.Layers != 1)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid texture dimensions or type"});
  }
  if (!Vulkan::Supports(context, description.Texels, description.Use, false)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "unsupported texture format or use"});
  }
  VkImageFormatProperties limits{};
  const auto result = vkGetPhysicalDeviceImageFormatProperties(context.Physical,
                                                               TexelFormat(description.Texels),
                                                               VK_IMAGE_TYPE_2D,
                                                               VK_IMAGE_TILING_OPTIMAL,
                                                               ImageUsage(description.Use),
                                                               0,
                                                               &limits);
  if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "unsupported image description"});
  }
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkGetPhysicalDeviceImageFormatProperties", result));
  }
  if (description.Width > limits.maxExtent.width || description.Height > limits.maxExtent.height ||
      description.Mips > limits.maxMipLevels || description.Layers > limits.maxArrayLayers) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "texture exceeds format limits"});
  }
  return {};
}

}

Result<std::shared_ptr<ImageState>> AllocateImage(std::shared_ptr<Context> context,
                                                  TextureDescription description) {
  if (auto owner = CheckOwner(*context); !owner) { return std::unexpected(owner.error()); }
  if (auto checked = CheckImage(*context, description); !checked) {
    return std::unexpected(checked.error());
  }
  auto state = std::make_shared<ImageState>();
  state->Owner = std::move(context);
  state->Description = description;
  state->Layouts.resize(static_cast<std::size_t>(description.Mips) * description.Layers,
                        VK_IMAGE_LAYOUT_UNDEFINED);
  VkImageCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  create.imageType = VK_IMAGE_TYPE_2D;
  create.format = TexelFormat(description.Texels);
  create.extent = {.width = description.Width, .height = description.Height, .depth = 1};
  create.mipLevels = description.Mips;
  create.arrayLayers = description.Layers;
  create.samples = VK_SAMPLE_COUNT_1_BIT;
  create.tiling = VK_IMAGE_TILING_OPTIMAL;
  create.usage = ImageUsage(description.Use);
  create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  auto result = vkCreateImage(state->Owner->Logical, &create, nullptr, &state->Handle);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateImage", result)); }
  VkMemoryRequirements requirements{};
  vkGetImageMemoryRequirements(state->Owner->Logical, state->Handle, &requirements);
  const auto memory = FindMemory(*state->Owner,
                                 requirements.memoryTypeBits,
                                 {.Required = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});
  if (!memory) { return std::unexpected(memory.error()); }
  VkMemoryAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocation.allocationSize = requirements.size;
  allocation.memoryTypeIndex = *memory;
  result = vkAllocateMemory(state->Owner->Logical, &allocation, nullptr, &state->Allocation);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkAllocateMemory(image)", result)); }
  result = vkBindImageMemory(state->Owner->Logical, state->Handle, state->Allocation, 0);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkBindImageMemory", result)); }
  return state;
}

Result<std::shared_ptr<ImageState>> ImageFrom(const Texture &image, const Context &context) {
  const auto *const native = dynamic_cast<const VulkanTexture *>(&image);
  if (native == nullptr || native->State()->Owner.get() != &context) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "texture belongs to another device"});
  }
  return native->State();
}

bool ValidView(const ImageState &image, TextureView view) noexcept {
  return view.Mips != 0 && view.Layers != 0 && view.FirstMip < image.Description.Mips &&
         view.Mips <= image.Description.Mips - view.FirstMip &&
         view.FirstLayer < image.Description.Layers &&
         view.Layers <= image.Description.Layers - view.FirstLayer;
}

Result<VkImageView> CreateView(const ImageState &image, TextureView view) {
  if (!ValidView(image, view)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid texture view range"});
  }
  VkImageViewCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  create.image = image.Handle;
  create.viewType = image.Description.Type == TextureType::Image2DArray
                        ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                        : VK_IMAGE_VIEW_TYPE_2D;
  create.format = TexelFormat(image.Description.Texels);
  create.subresourceRange = {.aspectMask = Aspect(image.Description.Texels),
                             .baseMipLevel = view.FirstMip,
                             .levelCount = view.Mips,
                             .baseArrayLayer = view.FirstLayer,
                             .layerCount = view.Layers};
  VkImageView native = VK_NULL_HANDLE;
  const auto result = vkCreateImageView(image.Owner->Logical, &create, nullptr, &native);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateImageView", result)); }
  return native;
}

bool ValidRegion(const ImageState &image, TextureRegion region) noexcept {
  if (region.Mip >= image.Description.Mips || region.Layer >= image.Description.Layers ||
      region.Width == 0 || region.Height == 0) {
    return false;
  }
  const auto width = std::max(1u, image.Description.Width >> region.Mip);
  const auto height = std::max(1u, image.Description.Height >> region.Mip);
  return region.X <= width && region.Width <= width - region.X && region.Y <= height &&
         region.Height <= height - region.Y;
}

}
#endif
