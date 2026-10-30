#include "BenchmarkReport.h"
#include "Calibration.h"
#include "Check.h"
#include <nlohmann/json.hpp>
#include <array>
#include <cstdint>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr uint64_t resident = 9007199254740993ULL;
  const ProcessMemorySnapshot memory{.ResidentBytes = resident,
                                     .PeakResidentBytes = resident + 5,
                                     .FootprintBytes = resident - 7,
                                     .PeakFootprintBytes = resident + 2,
                                     .CollectionNs = 14923};
  App::ScenarioBenchmarkReport scenario;
  Diagnostics::Calibration::Report synthetic;
  scenario.Memory = memory;
  synthetic.Memory = memory;
  const auto records = [&] {
    return std::array{
        nlohmann::json::parse(App::ScenarioBenchmarkJson(scenario), nullptr, false),
        nlohmann::json::parse(Diagnostics::Calibration::Json(synthetic), nullptr, false)};
  };
  auto reports = records();
  for (const auto &report : reports) {
    CHECK(!report.is_discarded() && report.contains("process_memory"),
          "both benchmark modes serialize the captured process observation");
    if (report.is_discarded() || !report.contains("process_memory")) { continue; }
    const auto &value = report["process_memory"];
    CHECK(value["resident_bytes"].is_number_unsigned() && value["resident_bytes"] == resident &&
              value["peak_resident_bytes"] == resident + 5,
          "byte counters preserve integer precision above the exact double range");
    CHECK(value["footprint_bytes"] == resident - 7 && value["peak_footprint_bytes"] == resident + 2,
          "footprint stays separate from residency without aggregation or substitution");
    CHECK(value["peak_scope"] == "process_lifetime" && value["collection_ns"] == 14923,
          "the high-water scope and collection cost are explicit");
  }
  if (!reports[0].contains("process_memory") || !reports[1].contains("process_memory")) {
    return Report();
  }
  CHECK(reports[0]["process_memory"]["sample_phase"] == "after_measurement" &&
            reports[1]["process_memory"]["sample_phase"] == "after_workloads",
        "both collectors name their own completed-work observation phase");
  scenario.Memory = {};
  synthetic.Memory = {};
  reports = records();
  for (const auto &report : reports) {
    for (const std::string_view field : {"resident_bytes",
                                         "peak_resident_bytes",
                                         "footprint_bytes",
                                         "peak_footprint_bytes",
                                         "collection_ns"}) {
      CHECK(report["process_memory"][field].is_null(),
            "unavailable observations remain null instead of fabricated zero values");
    }
  }
  return Report();
}
