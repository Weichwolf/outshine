#include "Calibration.h"
#include "Parameters.inc"
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string>

int main() {
  using namespace outshine::Diagnostics::Calibration;
  const auto result = CpuWorkloads();
  if (!result || result->size() != 8) { return 1; }
  constexpr std::array<uint32_t, 4> counts{1, 2, 4, 6};
  constexpr uint64_t expectedChecksum = 17031792103250770736ull;
  constexpr uint64_t expectedOperations =
      uint64_t{CALIBRATION_FMA_ELEMENTS} * CALIBRATION_FMA_STEPS * 4u * 2u;
  for (size_t at = 0; at < counts.size(); ++at) {
    const auto &work = (*result)[at];
    if (work.Name != "cpu_fma_" + std::to_string(counts[at]) + "t" || work.Threads != counts[at] ||
        work.TimeBasis != "host_wall_ns" || work.Elements != CALIBRATION_FMA_ELEMENTS ||
        work.Iterations != CALIBRATION_FMA_STEPS || work.LogicalOperations != expectedOperations ||
        work.Checksum != expectedChecksum || work.Samples.size() != CALIBRATION_SAMPLES) {
      return 1;
    }
    for (const auto &sample : work.Samples) {
      if (!(sample.WallNs > 0) || sample.GpuNs) { return 1; }
    }
  }
  std::println("CPU thread counts 1/2/4/6: equal work, names, samples and complete checksums PASS");
}
