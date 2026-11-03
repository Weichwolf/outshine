#include "BenchmarkReport.h"
#include "Calibration.h"
#include "Check.h"
#include <nlohmann/json.hpp>
#include <array>
#include <cstdint>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  App::ScenarioBenchmarkReport scenario;
  Diagnostics::Calibration::Report synthetic;
  const auto records = [&] {
    return std::array{
        nlohmann::json::parse(App::ScenarioBenchmarkJson(scenario), nullptr, false),
        nlohmann::json::parse(Diagnostics::Calibration::Json(synthetic), nullptr, false)};
  };
  for (const auto &report : records()) {
    CHECK(!report.is_discarded() && report.contains("gpu_memory"),
          "both benchmark modes emit a native device observation");
    if (report.is_discarded() || !report.contains("gpu_memory")) { continue; }
    const auto &memory = report["gpu_memory"];
    CHECK(memory["resource_allocated_bytes"].is_null() &&
              memory["recommended_working_set_bytes"].is_null() &&
              memory["unified_memory"].is_null() && memory["collection_ns"].is_null() &&
              memory["heaps"].empty(),
          "unavailable observations never masquerade as zero-byte allocations or known topology");
  }
  constexpr uint64_t bytes = 9007199254740993ULL;
  const Render::Native::MemoryStatus observed{.ResourceAllocatedBytes = bytes,
                                              .RecommendedWorkingSetBytes = bytes + 7,
                                              .UnifiedMemory = false,
                                              .CollectionNs = 257,
                                              .Heaps = {{.CapacityBytes = bytes + 17,
                                                         .DeviceLocal = true,
                                                         .HostVisible = true,
                                                         .EstimatedUsageBytes = bytes - 1,
                                                         .EstimatedBudgetBytes = bytes + 3},
                                                        {.CapacityBytes = bytes + 31,
                                                         .DeviceLocal = false,
                                                         .HostVisible = true,
                                                         .EstimatedUsageBytes = {},
                                                         .EstimatedBudgetBytes = {}}}};
  scenario.GpuMemory = observed;
  synthetic.GpuMemory = observed;
  const auto reports = records();
  for (const auto &report : reports) {
    const auto &memory = report["gpu_memory"];
    CHECK(memory["resource_allocated_bytes"].is_number_unsigned() &&
              memory["resource_allocated_bytes"] == bytes &&
              memory["recommended_working_set_bytes"] == bytes + 7 &&
              memory["unified_memory"] == false && memory["collection_ns"] == 257,
          "exact resource bytes and explicit false topology survive serialization without doubles");
    const auto &heap = memory["heaps"][0];
    CHECK(heap["capacity_bytes"] == bytes + 17 && heap["device_local"] == true &&
              heap["host_visible"] == true && heap["estimated_usage_bytes"] == bytes - 1 &&
              heap["estimated_budget_bytes"] == bytes + 3 &&
              memory["heaps"][1]["estimated_usage_bytes"].is_null() &&
              memory["heaps"][1]["estimated_budget_bytes"].is_null(),
          "heap capacity, estimates and unavailable extension data remain separate");
    CHECK(memory["scope"] == "device_snapshot" && !memory.contains("available_bytes") &&
              !memory.contains("total_process_bytes"),
          "a device snapshot invents neither allocation permission nor an overlapping UMA total");
  }
  CHECK(reports[0]["gpu_memory"]["sample_phase"] == "after_measurement" &&
            reports[1]["gpu_memory"]["sample_phase"] == "after_workloads",
        "the two completed-work collectors identify their observation phases");
  return Report();
}
