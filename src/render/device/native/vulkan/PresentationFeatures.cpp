#ifdef __linux__
#include "Surface.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace outshine::Render::Native::Vulkan {

namespace {

bool HasExtensions(VkPhysicalDevice physical, std::span<const char *const> names) {
  uint32_t count = 0;
  if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr) != VK_SUCCESS) {
    return false;
  }
  std::vector<VkExtensionProperties> available(count);
  if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, available.data()) !=
      VK_SUCCESS) {
    return false;
  }
  const auto has = [&](const char *name) {
    return std::ranges::any_of(available, [name](const VkExtensionProperties &item) {
      return std::strcmp(item.extensionName, name) == 0;
    });
  };
  return std::ranges::all_of(names, has);
}

}

bool HasPresentExtensions(VkPhysicalDevice physical) {
  constexpr std::array names{VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                             VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME};
  return HasExtensions(physical, names);
}

bool SupportsSharedPresentQueue([[maybe_unused]] VkPhysicalDevice physical) {
#ifdef VK_KHR_internally_synchronized_queues
  constexpr std::array names{VK_KHR_INTERNALLY_SYNCHRONIZED_QUEUES_EXTENSION_NAME};
  if (!HasExtensions(physical, names)) { return false; }
  VkPhysicalDeviceInternallySynchronizedQueuesFeaturesKHR synchronized{};
  synchronized.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INTERNALLY_SYNCHRONIZED_QUEUES_FEATURES_KHR;
  VkPhysicalDeviceFeatures2 features{};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &synchronized;
  vkGetPhysicalDeviceFeatures2(physical, &features);
  return synchronized.internallySynchronizedQueues != VK_FALSE;
#else
  return false;
#endif
}

}
#endif
