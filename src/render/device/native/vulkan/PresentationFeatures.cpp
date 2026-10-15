#ifdef __linux__
#include "Surface.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace outshine::Render::Native::Vulkan {

bool HasPresentExtensions(VkPhysicalDevice physical) {
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
  return has(VK_KHR_SWAPCHAIN_EXTENSION_NAME) && has(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
}

}
#endif
