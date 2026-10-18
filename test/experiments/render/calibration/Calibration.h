#ifndef OUTSHINE_DIAGNOSTICS_CALIBRATION_H
#define OUTSHINE_DIAGNOSTICS_CALIBRATION_H

#include "Device.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine::Diagnostics::Calibration {
namespace Native = Render::Native;

struct Sample {
  double WallNs = 0;
  std::optional<double> EncodeNs;
  std::optional<double> SubmitNs;
  std::optional<double> WaitNs;
  std::optional<double> GpuNs;
};

struct Workload {
  std::string Name;
  std::string TimeBasis;
  uint32_t Threads = 0;
  uint64_t Elements = 0;
  uint64_t Iterations = 0;
  uint64_t LogicalOperations = 0;
  uint64_t LogicalBytes = 0;
  uint64_t WorkingSetBytes = 0;
  uint64_t Checksum = 0;
  std::vector<Sample> Samples;
  std::string Note;
};

struct Shaders {
  std::span<const uint32_t> Fma;
  std::span<const uint32_t> Triad;
  std::span<const uint32_t> Vertex;
  std::span<const uint32_t> Fragment;
};

struct Build {
  std::string Commit;
  std::string Flags;
  std::string HostLoad;
};

struct Report {
  std::string Cpu;
  std::string Gpu;
  std::string Backend;
  std::string Compiler;
  std::string Architecture;
  uint64_t RamBytes = 0;
  uint32_t LogicalCpuCount = 0;
  bool FmaTargetFeature = false;
  Build Compilation;
  std::vector<Workload> Workloads;
};

using FmaValue = std::array<float, 4>;
static_assert(sizeof(FmaValue) == 4 * sizeof(float));
static_assert(sizeof(float) == sizeof(uint32_t));

[[nodiscard]] Native::Result<Report> Run(Native::Device &device, Shaders shaders, Build build);
[[nodiscard]] std::string Json(const Report &report);
[[nodiscard]] Native::Result<std::vector<Workload>> CpuWorkloads();
[[nodiscard]] Native::Result<std::vector<Workload>> GpuWorkloads(Native::Device &device,
                                                                 Shaders shaders);
[[nodiscard]] FmaValue FmaInput(uint32_t index) noexcept;
[[nodiscard]] FmaValue FmaReference(FmaValue value) noexcept;
void FmaKernel(std::span<const FmaValue> input, std::span<FmaValue> output) noexcept;
void TriadKernel(std::span<const uint32_t> a,
                 std::span<const uint32_t> b,
                 std::span<uint32_t> into) noexcept;
[[nodiscard]] uint32_t ChaseKernel(std::span<const uint32_t> next, uint32_t steps) noexcept;
[[nodiscard]] uint32_t RasterValue(uint32_t x, uint32_t y) noexcept;
[[nodiscard]] uint64_t Checksum(std::span<const std::byte> bytes) noexcept;
}
#endif
