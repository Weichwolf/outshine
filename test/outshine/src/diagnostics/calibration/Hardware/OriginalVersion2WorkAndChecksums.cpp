#include "Calibration.h"
#include "Check.h"
#include "test/harness/calibration/Version2Workloads.h"

#include <cmath>
#include <cstdio>
#include <print>

int main() {
  namespace Calibration = outshine::Diagnostics::Calibration;
  using namespace outshine::Test;
#ifdef OUTSHINE_GPU_VALIDATION
  constexpr bool validation = true;
#else
  constexpr bool validation = false;
#endif
  const auto result = Calibration::Run(validation);
  CHECK(result.has_value(), "the actual native CPU, memory, compute and raster workloads run");
  if (!result) {
    std::println(stderr, "calibration: {}", result.error().Message);
    return Report();
  }
  CHECK(result->GpuValidation == validation, "validation is declared in the native report");
  CHECK(result->Memory.CollectionNs && result->Memory.PeakResidentBytes,
        "the completed native workloads capture their actual process high-water observation");
  CHECK(!result->Cpu.empty() && !result->Gpu.empty() && !result->Compiler.empty(),
        "host processor, GPU and compiler have independent identities");
  const auto &expected = outshine::Test::Calibration::kVersion2Workloads;
  CHECK(result->Workloads.size() == expected.size(), "all twelve original workloads are present");
  if (result->Workloads.size() != expected.size()) { return Report(); }
  for (size_t at = 0; at < expected.size(); ++at) {
    const auto &actual = result->Workloads[at];
    const auto &old = expected[at];
    CHECK(actual.Name == old.Name && actual.Threads == old.Threads &&
              actual.Elements == old.Elements && actual.Iterations == old.Iterations &&
              actual.LogicalOperations == old.Operations && actual.LogicalBytes == old.Bytes &&
              actual.WorkingSetBytes == old.WorkingSet && actual.Checksum == old.Checksum,
          "original work, inputs and complete verified output checksum remain unchanged");
    CHECK(actual.Samples.size() == 5, "one preparation round leaves five measured samples");
    for (const auto &sample : actual.Samples) {
      CHECK(std::isfinite(sample.WallNs) && sample.WallNs > 0,
            "each sample contains a positive actual host duration");
      if (actual.Name.starts_with("cpu_")) {
        CHECK(!sample.GpuNs && !sample.WaitNs,
              "CPU work cannot acquire a made-up GPU or fence duration");
      } else if (sample.GpuNs) {
        CHECK(std::isfinite(*sample.GpuNs) && *sample.GpuNs > 0,
              "available GPU time is an actual positive submission interval");
      }
    }
  }
  const auto json = Calibration::Json(*result);
  CHECK(json.contains("\"workload_version\":2") && json.contains("\"verified\":true"),
        "the same native execution produces versioned verified JSON");
  return Report();
}
