#ifdef __linux__
#include "Context.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace outshine::Render::Native::Vulkan {
bool HasDeviceExtensions(VkPhysicalDevice physical, std::span<const char *const> names) {
  uint32_t count = 0;
  if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr) != VK_SUCCESS) {
    return false;
  }
  std::vector<VkExtensionProperties> available(count);
  if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, available.data()) !=
      VK_SUCCESS) {
    return false;
  }
  available.resize(count);
  return std::ranges::all_of(names, [&](const char *name) {
    return std::ranges::any_of(available, [name](const VkExtensionProperties &item) {
      return std::strcmp(item.extensionName, name) == 0;
    });
  });
}

}
#endif
