#ifdef __linux__
#include "Surface.h"
#include <array>

namespace outshine::Render::Native::Vulkan {

bool HasPresentExtensions(VkPhysicalDevice physical) {
  constexpr std::array names{VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                             VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME};
  return HasDeviceExtensions(physical, names);
}

bool SupportsSharedPresentQueue([[maybe_unused]] VkPhysicalDevice physical) {
#ifdef VK_KHR_internally_synchronized_queues
  constexpr std::array names{VK_KHR_INTERNALLY_SYNCHRONIZED_QUEUES_EXTENSION_NAME};
  if (!HasDeviceExtensions(physical, names)) { return false; }
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
