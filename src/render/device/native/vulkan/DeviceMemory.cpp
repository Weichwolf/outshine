#ifdef __linux__
#include "Context.h"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <ratio>

namespace outshine::Render::Native::Vulkan {
MemoryStatus ReadMemoryStatus(const Context &context) {
  assert(OnOwner(context));
  const auto began = std::chrono::steady_clock::now();
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
  budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
  VkPhysicalDeviceMemoryProperties2 properties{};
  properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
  if (context.MemoryBudgetEnabled) { properties.pNext = &budget; }
  vkGetPhysicalDeviceMemoryProperties2(context.Physical, &properties);
  const auto &memory = properties.memoryProperties;
  MemoryStatus status;
  status.Heaps.reserve(memory.memoryHeapCount);
  for (uint32_t at = 0; at < memory.memoryHeapCount; ++at) {
    auto &heap = status.Heaps.emplace_back();
    heap.CapacityBytes = memory.memoryHeaps[at].size;
    heap.DeviceLocal = (memory.memoryHeaps[at].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;
    for (uint32_t type = 0; type < memory.memoryTypeCount; ++type) {
      if (memory.memoryTypes[type].heapIndex == at &&
          (memory.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
        heap.HostVisible = true;
      }
    }
    if (context.MemoryBudgetEnabled) {
      heap.EstimatedUsageBytes = budget.heapUsage[at];
      heap.EstimatedBudgetBytes = budget.heapBudget[at];
    }
  }
  status.CollectionNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - began)
          .count());
  return status;
}
}
#endif
