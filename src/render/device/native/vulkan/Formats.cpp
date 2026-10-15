#ifdef __linux__
#include "VulkanImage.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace outshine::Render::Native::Vulkan {

VkFormat TexelFormat(Format format) noexcept {
  constexpr std::array formats = {VK_FORMAT_R8_UNORM,
                                  VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_FORMAT_R8G8B8A8_SRGB,
                                  VK_FORMAT_B8G8R8A8_UNORM,
                                  VK_FORMAT_B8G8R8A8_SRGB,
                                  VK_FORMAT_R16G16_SFLOAT,
                                  VK_FORMAT_R16G16B16A16_SFLOAT,
                                  VK_FORMAT_R32_SFLOAT,
                                  VK_FORMAT_R32G32B32A32_SFLOAT,
                                  VK_FORMAT_D32_SFLOAT};
  const auto index = static_cast<std::size_t>(format);
  return index < formats.size() ? formats[index] : VK_FORMAT_UNDEFINED;
}

uint32_t TexelBytes(Format format) noexcept {
  constexpr std::array<uint32_t, static_cast<std::size_t>(Format::Count)> bytes = {
      1, 4, 4, 4, 4, 4, 8, 4, 16, 4};
  const auto index = static_cast<std::size_t>(format);
  return index < bytes.size() ? bytes[index] : 0;
}

VkImageAspectFlags Aspect(Format format) noexcept {
  return format == Format::Depth32Float ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageUsageFlags ImageUsage(UseMask<TextureUse> use) noexcept {
  const auto bits = static_cast<uint32_t>(use);
  constexpr std::array flags = {VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                VK_IMAGE_USAGE_SAMPLED_BIT,
                                VK_IMAGE_USAGE_STORAGE_BIT,
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};
  VkImageUsageFlags flagsOut = 0;
  for (std::size_t bit = 0; bit < flags.size(); ++bit) {
    if ((bits & (1u << bit)) != 0) { flagsOut |= flags[bit]; }
  }
  return flagsOut;
}

namespace {

bool TypedStorageFormat(const Context &context, Format format) {
  switch (format) {
    case Format::Rgba8Unorm:
    case Format::Rgba16Float:
    case Format::R32Float:
    case Format::Rgba32Float: return true;
    case Format::R8Unorm:
    case Format::Rg16Float: return context.StorageImageExtendedFormats;
    default: return false;
  }
}

}

bool Supports(const Context &context, Format format, UseMask<TextureUse> use, bool filtered) {
  constexpr uint32_t knownUses = (1u << 6u) - 1;
  const auto bits = static_cast<uint32_t>(use);
  const VkFormat native = TexelFormat(format);
  if (!OnOwner(context) || native == VK_FORMAT_UNDEFINED || bits == 0 || (bits & ~knownUses) != 0) {
    return false;
  }
  if ((format == Format::Depth32Float &&
       (bits & static_cast<uint32_t>(TextureUse::ColourAttachment)) != 0) ||
      (format != Format::Depth32Float &&
       (bits & static_cast<uint32_t>(TextureUse::DepthAttachment)) != 0)) {
    return false;
  }
  if ((bits & static_cast<uint32_t>(TextureUse::Storage)) != 0 &&
      !TypedStorageFormat(context, format)) {
    return false;
  }
  VkFormatProperties properties{};
  vkGetPhysicalDeviceFormatProperties(context.Physical, native, &properties);
  constexpr std::array required = {VK_FORMAT_FEATURE_TRANSFER_SRC_BIT,
                                   VK_FORMAT_FEATURE_TRANSFER_DST_BIT,
                                   VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
                                   VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,
                                   VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,
                                   VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT};
  VkFormatFeatureFlags features = filtered ? VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT : 0;
  for (std::size_t bit = 0; bit < required.size(); ++bit) {
    if ((bits & (1u << bit)) != 0) { features |= required[bit]; }
  }
  return (properties.optimalTilingFeatures & features) == features;
}

}
#endif
