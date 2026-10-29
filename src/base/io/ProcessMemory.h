#ifndef OUTSHINE_BASE_IO_PROCESSMEMORY_H
#define OUTSHINE_BASE_IO_PROCESSMEMORY_H

#include <cstdint>
#include <optional>

namespace outshine {

struct ProcessMemorySnapshot {
  std::optional<uint64_t> ResidentBytes;
  std::optional<uint64_t> PeakResidentBytes;
  std::optional<uint64_t> FootprintBytes;
  std::optional<uint64_t> PeakFootprintBytes;
  std::optional<uint64_t> CollectionNs;
};

// Peaks cover process lifetime; footprint overlaps RSS and is not a GPU total.
[[nodiscard]] ProcessMemorySnapshot ReadProcessMemory() noexcept;

}

#endif
