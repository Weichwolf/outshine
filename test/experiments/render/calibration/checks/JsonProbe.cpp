#include "Calibration.h"
#include <limits>
#include <print>
#include <utility>

int main() {
  using namespace outshine::Diagnostics::Calibration;
  Report report{};
  report.Cpu = "quote\" newline\n tab\t slash\\";
  Workload unavailable{};
  unavailable.Name = "gpu_without_timestamp";
  unavailable.TimeBasis = "submission_gpu_ns";
  unavailable.LogicalOperations = 4;
  Sample sample{};
  sample.WallNs = 8;
  sample.WaitNs = 5;
  unavailable.Samples.push_back(sample);
  sample.GpuNs = std::numeric_limits<double>::quiet_NaN();
  unavailable.Samples.push_back(sample);
  report.Workloads.push_back(std::move(unavailable));
  Workload cpu{};
  cpu.Name = "cpu_chase_test";
  cpu.TimeBasis = "host_wall_ns";
  cpu.Iterations = 2;
  for (const auto ns : {30., 10., 20.}) {
    Sample measured{};
    measured.WallNs = ns;
    cpu.Samples.push_back(measured);
  }
  report.Workloads.push_back(std::move(cpu));
  std::print("{}", Json(report));
}
