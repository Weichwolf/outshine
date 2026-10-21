#ifdef __linux__
#include "Presenter.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace outshine::Render::Native::Vulkan {

SwapchainState::~SwapchainState() {
  if (Handle != VK_NULL_HANDLE) { vkDestroySwapchainKHR(Owner->Logical, Handle, nullptr); }
}

namespace {

Result<VkSurfaceFormatKHR> SurfaceFormat(const SurfaceState &surface) {
  uint32_t count = 0;
  auto result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      surface.Owner->Physical, surface.Handle, &count, nullptr);
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkGetPhysicalDeviceSurfaceFormatsKHR", result));
  }
  std::vector<VkSurfaceFormatKHR> formats(count);
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      surface.Owner->Physical, surface.Handle, &count, formats.data());
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkGetPhysicalDeviceSurfaceFormatsKHR", result));
  }
  constexpr std::array preferred{VK_FORMAT_B8G8R8A8_SRGB,
                                 VK_FORMAT_R8G8B8A8_SRGB,
                                 VK_FORMAT_B8G8R8A8_UNORM,
                                 VK_FORMAT_R8G8B8A8_UNORM};
  for (const auto texels : preferred) {
    const auto found = std::ranges::find_if(formats, [texels](const VkSurfaceFormatKHR &format) {
      return format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
             (format.format == texels || format.format == VK_FORMAT_UNDEFINED);
    });
    if (found != formats.end()) {
      return VkSurfaceFormatKHR{.format = texels, .colorSpace = found->colorSpace};
    }
  }
  return std::unexpected(
      Error{.Kind = ErrorKind::Unsupported, .Message = "no supported SDR Vulkan surface format"});
}

Format NativeFormat(VkFormat format) {
  switch (format) {
    case VK_FORMAT_B8G8R8A8_UNORM: return Format::Bgra8Unorm;
    case VK_FORMAT_R8G8B8A8_UNORM: return Format::Rgba8Unorm;
    case VK_FORMAT_B8G8R8A8_SRGB: return Format::Bgra8Srgb;
    case VK_FORMAT_R8G8B8A8_SRGB: return Format::Rgba8Srgb;
    default: return Format::Bgra8Unorm;
  }
}

VkCompositeAlphaFlagBitsKHR Composite(VkCompositeAlphaFlagsKHR supported) {
  constexpr std::array modes{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                             VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
                             VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                             VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR};
  for (const auto mode : modes) {
    if ((supported & mode) != 0) { return mode; }
  }
  return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

}

Result<VkSurfaceCapabilitiesKHR> ReadSurfaceCapabilities(const SurfaceState &surface) {
  VkSurfaceCapabilitiesKHR caps{.minImageCount = 0,
                                .maxImageCount = 0,
                                .currentExtent = {},
                                .minImageExtent = {},
                                .maxImageExtent = {},
                                .maxImageArrayLayers = 0,
                                .supportedTransforms = 0,
                                .currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                                .supportedCompositeAlpha = 0,
                                .supportedUsageFlags = 0};
  const auto result =
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(surface.Owner->Physical, surface.Handle, &caps);
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", result));
  }
  return caps;
}

Result<std::shared_ptr<SwapchainState>>
CreateSwapchain(std::shared_ptr<SurfaceState> surface,
                VkExtent2D extent,
                uint64_t generation,
                const std::shared_ptr<SwapchainState> &before) {
  const auto &context = surface->Owner;
  const auto supported = ReadSurfaceCapabilities(*surface);
  if (!supported) { return std::unexpected(supported.error()); }
  const auto &caps = *supported;
  const auto format = SurfaceFormat(*surface);
  if (!format) { return std::unexpected(format.error()); }
  if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    extent = caps.currentExtent;
  }
  extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
  extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
  if ((caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                 .Message = "Vulkan surface has no colour-attachment support"});
  }
  if (extent.width == 0 || extent.height == 0) {
    return std::unexpected(Error{.Kind = ErrorKind::Busy,
                                 .Message = "Vulkan surface currently has no drawable extent"});
  }
  auto state = std::make_shared<SwapchainState>();
  state->Owner = context;
  state->Surface = std::move(surface);
  state->Extent = extent;
  state->Generation = generation;
  state->Texels = NativeFormat(format->format);
  VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if ((caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0) {
    usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    state->Use = state->Use | UseMask(TextureUse::CopySource);
  }
  uint32_t count = std::max(3u, caps.minImageCount);
  if (caps.maxImageCount != 0) { count = std::min(count, caps.maxImageCount); }
  const VkSwapchainCreateInfoKHR create{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .surface = state->Surface->Handle,
      .minImageCount = count,
      .imageFormat = format->format,
      .imageColorSpace = format->colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = usage,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .preTransform = caps.currentTransform,
      .compositeAlpha = Composite(caps.supportedCompositeAlpha),
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .clipped = (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0 ? VK_FALSE : VK_TRUE,
      .oldSwapchain = before ? before->Handle : VK_NULL_HANDLE};
  if (before) { before->DriverRetired = true; }
  auto result = vkCreateSwapchainKHR(state->Owner->Logical, &create, nullptr, &state->Handle);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateSwapchainKHR", result)); }
  result = vkGetSwapchainImagesKHR(state->Owner->Logical, state->Handle, &count, nullptr);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkGetSwapchainImagesKHR", result)); }
  state->Images.resize(count);
  result =
      vkGetSwapchainImagesKHR(state->Owner->Logical, state->Handle, &count, state->Images.data());
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkGetSwapchainImagesKHR", result)); }
  state->Layouts.resize(count, VK_IMAGE_LAYOUT_UNDEFINED);
  return state;
}

}
#endif
