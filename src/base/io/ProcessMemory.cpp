#include "ProcessMemory.h"
#include <chrono>
#include <cstdint>
#include <sys/resource.h>
#ifdef __APPLE__
#include <libproc.h>
#include <unistd.h>
#else
#include <array>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <system_error>
#include <unistd.h>
#endif

namespace outshine {
namespace {
#ifndef __APPLE__
std::optional<uint64_t> ResidentBytes() noexcept {
  auto *const file = std::fopen("/proc/self/statm", "r");
  if (file == nullptr) { return {}; }
  std::array<char, 128> line{};
  const bool read = std::fgets(line.data(), static_cast<int>(line.size()), file) != nullptr;
  (void)std::fclose(file);
  if (!read) { return {}; }
  const char *const end = line.data() + std::strlen(line.data());
  uint64_t virtualPages = 0;
  uint64_t residentPages = 0;
  const auto first = std::from_chars(line.data(), end, virtualPages);
  if (first.ec != std::errc{}) { return {}; }
  const char *at = first.ptr;
  while (at != end && (*at == ' ' || *at == '\t')) { ++at; }
  const auto resident = std::from_chars(at, end, residentPages);
  const long pageSize = sysconf(_SC_PAGESIZE);
  if (resident.ec != std::errc{} || pageSize <= 0 ||
      residentPages > std::numeric_limits<uint64_t>::max() / static_cast<uint64_t>(pageSize)) {
    return {};
  }
  return residentPages * static_cast<uint64_t>(pageSize);
}
#endif
}

ProcessMemorySnapshot ReadProcessMemory() noexcept {
  const auto began = std::chrono::steady_clock::now();
  ProcessMemorySnapshot sample;
#ifdef __APPLE__
  rusage_info_v4 usage{};
  if (proc_pid_rusage(getpid(), RUSAGE_INFO_V4, reinterpret_cast<rusage_info_t *>(&usage)) == 0) {
    sample.ResidentBytes = usage.ri_resident_size;
    sample.FootprintBytes = usage.ri_phys_footprint;
    sample.PeakFootprintBytes = usage.ri_lifetime_max_phys_footprint;
  }
#else
  sample.ResidentBytes = ResidentBytes();
#endif
  rusage peak{};
  if (getrusage(RUSAGE_SELF, &peak) == 0 && peak.ru_maxrss > 0) {
#ifdef __APPLE__
    sample.PeakResidentBytes = static_cast<uint64_t>(peak.ru_maxrss);
#else
    if (static_cast<uint64_t>(peak.ru_maxrss) <= std::numeric_limits<uint64_t>::max() / 1024u) {
      sample.PeakResidentBytes = static_cast<uint64_t>(peak.ru_maxrss) * 1024u;
    }
#endif
  }
  sample.CollectionNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - began)
          .count());
  return sample;
}

}
