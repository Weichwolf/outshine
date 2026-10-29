#include "ProcessMemory.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto sample = ReadProcessMemory();
  CHECK(sample.CollectionNs.has_value(), "native observation records its collection cost");
  CHECK(sample.PeakResidentBytes && *sample.PeakResidentBytes > 0,
        "the supported native process has a positive lifetime resident high-water mark");
#ifdef __APPLE__
  CHECK(sample.ResidentBytes && *sample.ResidentBytes > 0 && sample.FootprintBytes &&
            *sample.FootprintBytes > 0 && sample.PeakFootprintBytes,
        "macOS provides independent resident and footprint observations");
  CHECK(sample.FootprintBytes && sample.PeakFootprintBytes &&
            *sample.PeakFootprintBytes >= *sample.FootprintBytes,
        "the same native footprint observation preserves its lifetime high-water mark");
#else
  CHECK(!sample.FootprintBytes && !sample.PeakFootprintBytes,
        "a Linux resident count is never substituted for an unavailable footprint ledger");
#endif
  return Report();
}
