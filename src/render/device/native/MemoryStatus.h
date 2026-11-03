#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_MEMORYSTATUS_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_MEMORYSTATUS_H
#include <cstdint>
#include <optional>
#include <vector>

namespace outshine::Render::Native {
struct MemoryHeapStatus {
  uint64_t CapacityBytes = 0;
  bool DeviceLocal = false;
  bool HostVisible = false;
  std::optional<uint64_t> EstimatedUsageBytes;
  std::optional<uint64_t> EstimatedBudgetBytes;
};

struct MemoryStatus {
  // Resource allocations and driver heap estimates have different scopes; neither is free RAM.
  std::optional<uint64_t> ResourceAllocatedBytes;
  std::optional<uint64_t> RecommendedWorkingSetBytes;
  std::optional<bool> UnifiedMemory;
  std::optional<uint64_t> CollectionNs;
  std::vector<MemoryHeapStatus> Heaps;
};
}
#endif
