#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_CONTEXT_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_CONTEXT_H

#ifdef __linux__
#include "Device.h"
#include <vulkan/vulkan.h>
#include <atomic>
#include <memory>
#include <string_view>
#include <thread>
#include <string>
#include <vector>

namespace outshine::Render::Native::Vulkan {

struct Context {
  VkInstance Instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT Debug = VK_NULL_HANDLE;
  VkPhysicalDevice Physical = VK_NULL_HANDLE;
  VkDevice Logical = VK_NULL_HANDLE;
  VkQueue Queue = VK_NULL_HANDLE;
  VkQueue PresentQueue = VK_NULL_HANDLE;
  VkSurfaceKHR Surface = VK_NULL_HANDLE;
  std::vector<std::string> InstanceExtensions;
  bool WsiEnabled = false;
  bool PresentCapable = false;
  bool HasPresentTarget = false;
  uint32_t QueueCount = 0;
  PFN_vkReleaseSwapchainImagesEXT ReleaseImages = nullptr;
  uint32_t QueueFamily = 0;
  uint32_t TimestampValidBits = 0;
  bool GpuTiming = false;
  uint64_t Cookie = 0;
  bool DebugMarkers = false;
  bool StorageImageExtendedFormats = false;
  bool IndependentBlend = false;
  bool MultiDrawIndirect = false;
  bool IndirectFirstInstance = false;
  std::atomic<uint32_t> ValidationErrors{0};
  const std::thread::id OwnerThread = std::this_thread::get_id();
  uint32_t Recordings = 0;
  uint32_t OpenRecordings = 0;
  VkPhysicalDeviceProperties Properties{};
  VkPhysicalDeviceMemoryProperties MemoryProperties{};
  Capabilities Limits;

  Context() = default;
  ~Context();
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;
};

[[nodiscard]] Error Failure(std::string_view operation, VkResult result);
[[nodiscard]] bool OnOwner(const Context &context) noexcept;
[[nodiscard]] Result<void> CheckOwner(const Context &context);
[[nodiscard]] Result<std::shared_ptr<Context>> CreateContext(DeviceOptions options);

struct MemoryChoice {
  VkMemoryPropertyFlags Required = 0;
  VkMemoryPropertyFlags Preferred = 0;
};

[[nodiscard]] Result<uint32_t>
FindMemory(const Context &context, uint32_t allowed, MemoryChoice choice);

}
#endif
#endif
