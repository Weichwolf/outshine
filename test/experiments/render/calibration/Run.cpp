#include "Calibration.h"
#include <SDL3/SDL_cpuinfo.h>
#include <algorithm>
#include <fstream>
#include <cstdint>
#include <expected>
#include <iterator>
#include <string>
#include <utility>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace outshine::Diagnostics::Calibration {
namespace {
std::string CpuName() {
#ifdef __APPLE__
  size_t bytes = 0;
  if (sysctlbyname("machdep.cpu.brand_string", nullptr, &bytes, nullptr, 0) == 0 && bytes > 1) {
    std::string name(bytes, '\0');
    if (sysctlbyname("machdep.cpu.brand_string", name.data(), &bytes, nullptr, 0) == 0) {
      name.resize(bytes - 1);
      return name;
    }
  }
#else
  std::ifstream file("/proc/cpuinfo");
  std::string line;
  while (std::getline(file, line)) {
    if (line.starts_with("model name")) {
      const auto colon = line.find(':');
      if (colon != std::string::npos) {
        const auto first = line.find_first_not_of(" \t", colon + 1);
        if (first != std::string::npos) { return line.substr(first); }
      }
    }
  }
#endif
  return "unknown";
}
}

Native::Result<Report> Run(Native::Device &device, Shaders shaders, Build build) {
  Report report{};
  report.Cpu = CpuName();
  report.Gpu = device.Limits().Name;
  report.Compilation = std::move(build);
  report.LogicalCpuCount = static_cast<uint32_t>(std::max(0, SDL_GetNumLogicalCPUCores()));
  report.RamBytes = static_cast<uint64_t>(std::max(0, SDL_GetSystemRAM())) * 1024u * 1024u;
#ifdef __clang__
  report.Compiler = "Clang " __clang_version__;
#else
  report.Compiler = __VERSION__;
#endif
#if defined(__aarch64__) || defined(__arm64__)
  report.Architecture = "aarch64";
  report.FmaTargetFeature = true;
#elifdef __x86_64__
  report.Architecture = "x86_64";
#ifdef __FMA__
  report.FmaTargetFeature = true;
#endif
#else
  report.Architecture = "unknown";
#endif
#ifdef __APPLE__
  report.Backend = "Metal";
#else
  report.Backend = "Vulkan";
#endif
  auto cpu = CpuWorkloads();
  if (!cpu) { return std::unexpected(cpu.error()); }
  report.Workloads = std::move(*cpu);
  auto gpu = GpuWorkloads(device, shaders);
  if (!gpu) { return std::unexpected(gpu.error()); }
  report.Workloads.insert(report.Workloads.end(),
                          std::make_move_iterator(gpu->begin()),
                          std::make_move_iterator(gpu->end()));
  return report;
}
}
