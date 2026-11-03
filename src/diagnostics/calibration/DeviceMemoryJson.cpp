#include "DeviceMemoryJson.h"
#include <nlohmann/json.hpp>
#include <utility>
#include <string_view>

namespace outshine::Diagnostics::Calibration {
nlohmann::json DeviceMemoryJson(const Render::Native::MemoryStatus &status,
                                std::string_view phase) {
  using Json = nlohmann::json;
  const auto value = [](const auto &field) { return field ? Json(*field) : Json(nullptr); };
  auto heaps = Json::array();
  for (const auto &heap : status.Heaps) {
    heaps.push_back({{"capacity_bytes", heap.CapacityBytes},
                     {"device_local", heap.DeviceLocal},
                     {"host_visible", heap.HostVisible},
                     {"estimated_usage_bytes", value(heap.EstimatedUsageBytes)},
                     {"estimated_budget_bytes", value(heap.EstimatedBudgetBytes)}});
  }
  return {{"sample_phase", phase},
          {"scope", "device_snapshot"},
          {"resource_allocated_bytes", value(status.ResourceAllocatedBytes)},
          {"recommended_working_set_bytes", value(status.RecommendedWorkingSetBytes)},
          {"unified_memory", value(status.UnifiedMemory)},
          {"collection_ns", value(status.CollectionNs)},
          {"heaps", std::move(heaps)}};
}
}
