#include "Calibration.h"
#include "Parameters.inc"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <chrono>
#include <ratio>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Diagnostics::Calibration {
namespace {
std::string Quote(std::string_view value) {
  std::string result = "\"";
  for (const unsigned char byte : value) {
    if (byte == '"' || byte == '\\') {
      result += '\\';
      result += static_cast<char>(byte);
    } else if (byte < 32) {
      result += std::format("\\u{:04x}", byte);
    } else {
      result += static_cast<char>(byte);
    }
  }
  return result + '"';
}

std::string Number(std::optional<double> value) {
  return value && std::isfinite(*value) ? std::format("{}", *value) : "null";
}

std::string Summary(const Workload &work) {
  std::vector<double> times;
  for (const auto &sample : work.Samples) {
    const auto time =
        work.TimeBasis == "submission_gpu_ns" ? sample.GpuNs : std::optional(sample.WallNs);
    if (time && std::isfinite(*time) && *time > 0) { times.push_back(*time); }
  }
  if (times.empty()) { return "null"; }
  std::ranges::sort(times);
  const size_t middle = times.size() / 2;
  const double median =
      times.size() % 2 != 0 ? times[middle] : (times[middle - 1] + times[middle]) / 2;
  return std::format(
      "{{\"available_samples\":{},\"min_ns\":{},\"median_ns\":{},\"max_ns\":{},"
      "\"logical_ops_per_second\":{},\"logical_bytes_per_second\":{},\"ns_per_access\":{}}}",
      times.size(),
      times.front(),
      median,
      times.back(),
      work.LogicalOperations != 0 ? Number(static_cast<double>(work.LogicalOperations) *
                                           static_cast<double>(std::nano::den) / median)
                                  : "null",
      work.LogicalBytes != 0 ? Number(static_cast<double>(work.LogicalBytes) *
                                      static_cast<double>(std::nano::den) / median)
                             : "null",
      work.Name.starts_with("cpu_chase_") ? Number(median / static_cast<double>(work.Iterations))
                                          : "null");
}

std::string Work(const Workload &work) {
  std::string result = std::format(
      "{{\"name\":{},\"time_basis\":{},\"threads\":{},\"elements\":{},\"iterations\":{},"
      "\"logical_operations\":{},\"logical_bytes\":{},\"working_set_bytes\":{},\"checksum\":{},"
      "\"note\":{},\"summary\":{},\"samples\":[",
      Quote(work.Name),
      Quote(work.TimeBasis),
      work.Threads,
      work.Elements,
      work.Iterations,
      work.LogicalOperations,
      work.LogicalBytes,
      work.WorkingSetBytes,
      work.Checksum,
      Quote(work.Note),
      Summary(work));
  bool first = true;
  for (const auto &sample : work.Samples) {
    if (!first) { result += ','; }
    first = false;
    result +=
        std::format(R"({{"wall_ns":{},"encode_ns":{},"submit_ns":{},"wait_ns":{},"gpu_ns":{}}})",
                    Number(sample.WallNs),
                    Number(sample.EncodeNs),
                    Number(sample.SubmitNs),
                    Number(sample.WaitNs),
                    Number(sample.GpuNs));
  }
  return result + "]}";
}
}

std::string Json(const Report &report) {
  std::string result = std::format(
      "{{\"workload_version\":{},\"verified\":true,\"warmup_samples\":1,\"measured_samples\":{},"
      "\"cpu\":{},\"gpu\":{},\"backend\":{},\"compiler\":{},\"architecture\":{},"
      "\"ram_bytes\":{},\"logical_cpu_count\":{},\"fma_target_feature\":{},"
      "\"build\":{{\"commit\":{},\"flags\":{},\"host_load\":{}}},\"workloads\":[",
      CALIBRATION_VERSION,
      CALIBRATION_SAMPLES,
      Quote(report.Cpu),
      Quote(report.Gpu),
      Quote(report.Backend),
      Quote(report.Compiler),
      Quote(report.Architecture),
      report.RamBytes,
      report.LogicalCpuCount,
      report.FmaTargetFeature,
      Quote(report.Compilation.Commit),
      Quote(report.Compilation.Flags),
      Quote(report.Compilation.HostLoad));
  bool first = true;
  for (const auto &work : report.Workloads) {
    if (!first) { result += ','; }
    first = false;
    result += Work(work);
  }
  return result + "]}\n";
}
}
